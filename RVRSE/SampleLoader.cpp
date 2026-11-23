/// @file SampleLoader.cpp
/// @brief Implementation of sample loading functions.
///        OFFLINE ONLY — never call from the audio thread.

#include "SampleLoader.h"
#include "Constants.h"
#include "dr_wav.h"
#include "dr_mp3.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>

namespace rvrse {

namespace {

constexpr const char* kSupportedFormatsMessage =
  "RVRSE supports WAV, AIFF and MP3 audio.";
constexpr const char* kCompressedWavMessage =
  "RVRSE supports uncompressed WAV and AIFF only. The selected WAV file uses compressed or unsupported encoding.";
constexpr const char* kUnreadableFormatsMessage =
  "RVRSE supports WAV, AIFF and MP3 audio. The selected file could not be read.";

bool IsSupportedWavFormatTag(drwav_uint16 formatTag)
{
  return formatTag == DR_WAVE_FORMAT_PCM || formatTag == DR_WAVE_FORMAT_IEEE_FLOAT;
}

std::string GetAudioFileLoadError(const drwav& wav)
{
  if (wav.container == drwav_container_aiff)
  {
    if (wav.translatedFormatTag != DR_WAVE_FORMAT_PCM)
      return kSupportedFormatsMessage;
  }
  else
  {
    if (!IsSupportedWavFormatTag(wav.translatedFormatTag))
      return kCompressedWavMessage;
  }

  return "";
}

} // namespace

SampleLoadResult LoadSample(const std::string& filePath)
{
  SampleLoadResult result;

  if (filePath.empty())
  {
    result.errorMessage = "Empty file path";
    return result;
  }

  if (!IsSupportedAudioFile(filePath))
  {
    result.errorMessage = kSupportedFormatsMessage;
    return result;
  }

  std::string extension = std::filesystem::path(filePath).extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension == ".mp3")
  {
    drmp3 mp3 {};
    if (!drmp3_init_file(&mp3, filePath.c_str(), nullptr))
    {
      result.errorMessage = "Failed to decode MP3: " + filePath;
      return result;
    }
    const auto channels = mp3.channels;
    const auto sampleRate = mp3.sampleRate;
    const auto frames = drmp3_get_pcm_frame_count(&mp3);
    if (channels == 0 || sampleRate == 0 || frames == 0 ||
        frames > static_cast<drmp3_uint64>(sampleRate) * kMaxSampleLengthSeconds ||
        frames > static_cast<drmp3_uint64>(std::numeric_limits<int>::max()))
    {
      drmp3_uninit(&mp3);
      result.errorMessage = "Invalid or excessively long MP3 audio";
      return result;
    }
    std::vector<float> pcm(static_cast<size_t>(frames) * channels);
    const auto read = drmp3_read_pcm_frames_f32(&mp3, frames, pcm.data());
    drmp3_uninit(&mp3);
    if (read == 0)
    {
      result.errorMessage = "Could not read MP3 samples";
      return result;
    }
    result.data.mLeft.resize(static_cast<size_t>(read));
    result.data.mRight.resize(static_cast<size_t>(read));
    for (size_t i = 0; i < static_cast<size_t>(read); ++i)
    {
      result.data.mLeft[i] = pcm[i * channels];
      result.data.mRight[i] = pcm[i * channels + (channels > 1 ? 1 : 0)];
    }
    result.data.mSampleRate = static_cast<double>(sampleRate);
    result.data.mNumChannels = static_cast<int>(std::min(channels, 2u));
    result.data.mFilePath = filePath;
    result.data.mFileName = ExtractFileName(filePath);
    result.success = true;
    return result;
  }

  // dr_wav handles both WAV and AIFF containers
  drwav wav;
  if (!drwav_init_file(&wav, filePath.c_str(), nullptr))
  {
    result.errorMessage = "Failed to open audio file: " + filePath;
    return result;
  }

  const std::string loadError = GetAudioFileLoadError(wav);
  if (!loadError.empty())
  {
    drwav_uninit(&wav);
    result.errorMessage = loadError;
    return result;
  }

  const drwav_uint64 totalFrames = wav.totalPCMFrameCount;
  const unsigned int channels = wav.channels;
  const unsigned int sampleRate = wav.sampleRate;

  // Sanity checks
  if (totalFrames == 0)
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file is empty (0 frames)";
    return result;
  }

  const drwav_uint64 maxFramesForRate = static_cast<drwav_uint64>(sampleRate) * kMaxSampleLengthSeconds;
  if (totalFrames > maxFramesForRate)
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file too long (max " + std::to_string(kMaxSampleLengthSeconds) + " seconds at " + std::to_string(sampleRate) + " Hz)";
    return result;
  }

  if (channels == 0)
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file has 0 channels";
    return result;
  }

  // Read all frames as interleaved float32
  const size_t interleavedSize = static_cast<size_t>(totalFrames) * channels;
  std::vector<float> interleaved(interleavedSize);

  const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(&wav, totalFrames, interleaved.data());
  drwav_uninit(&wav);

  if (framesRead == 0)
  {
    result.errorMessage = kUnreadableFormatsMessage;
    return result;
  }

  // Deinterleave into separate L/R channels
  const int numFrames = static_cast<int>(framesRead);
  result.data.mLeft.resize(numFrames);
  result.data.mRight.resize(numFrames);

  if (channels == 1)
  {
    // Mono: duplicate to both channels
    std::copy(interleaved.begin(), interleaved.begin() + numFrames, result.data.mLeft.begin());
    std::copy(interleaved.begin(), interleaved.begin() + numFrames, result.data.mRight.begin());
  }
  else
  {
    // Stereo (or more): deinterleave first 2 channels
    for (int i = 0; i < numFrames; ++i)
    {
      result.data.mLeft[i]  = interleaved[static_cast<size_t>(i) * channels];
      result.data.mRight[i] = interleaved[static_cast<size_t>(i) * channels + 1];
    }
  }

  result.data.mSampleRate = static_cast<double>(sampleRate);
  result.data.mNumChannels = static_cast<int>(std::min(channels, 2u));
  result.data.mFilePath = filePath;
  result.data.mFileName = ExtractFileName(filePath);

  result.success = true;
  return result;
}

std::string ExtractFileName(const std::string& filePath)
{
  // Find last path separator (works on both macOS/Linux and Windows)
  const auto posSlash = filePath.find_last_of('/');
  const auto posBackslash = filePath.find_last_of('\\');

  size_t pos = std::string::npos;
  if (posSlash != std::string::npos && posBackslash != std::string::npos)
    pos = std::max(posSlash, posBackslash);
  else if (posSlash != std::string::npos)
    pos = posSlash;
  else if (posBackslash != std::string::npos)
    pos = posBackslash;

  if (pos != std::string::npos)
    return filePath.substr(pos + 1);

  return filePath;
}

bool IsSupportedAudioFile(const std::string& filePath)
{
  if (filePath.size() < 4)
    return false;

  // Find the extension
  const auto dotPos = filePath.find_last_of('.');
  if (dotPos == std::string::npos)
    return false;

  std::string ext = filePath.substr(dotPos);

  // Convert to lowercase for comparison
  std::transform(ext.begin(), ext.end(), ext.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  return (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3");
}

std::string GetAudioFileLoadError(const std::string& filePath)
{
  if (filePath.empty())
    return "Empty file path";

  if (!IsSupportedAudioFile(filePath))
    return kSupportedFormatsMessage;

  if (!std::filesystem::is_regular_file(filePath))
    return "";

  std::string ext = std::filesystem::path(filePath).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext == ".mp3") return ""; // Decoder reports invalid files during LoadSample.

  drwav wav;
  if (!drwav_init_file(&wav, filePath.c_str(), nullptr))
    return kUnreadableFormatsMessage;

  const std::string result = GetAudioFileLoadError(wav);
  drwav_uninit(&wav);
  return result;
}

} // namespace rvrse
