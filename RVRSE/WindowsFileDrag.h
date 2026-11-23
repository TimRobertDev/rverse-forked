#pragma once
// Native Windows file drag using CF_HDROP. Called only from the GUI thread.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ole2.h>
#include <shlobj.h>
#include <string>
#include <cstring>

namespace rvrse {
class FileDragDataObject final : public IDataObject {
  LONG refs_ = 1;
  std::wstring path_;
public:
  explicit FileDragDataObject(std::wstring path) : path_(std::move(path)) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid == IID_IUnknown || iid == IID_IDataObject) { *out = static_cast<IDataObject*>(this); AddRef(); return S_OK; }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
  ULONG STDMETHODCALLTYPE Release() override { auto n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
  HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fmt, STGMEDIUM* med) override {
    if (!fmt || !med) return E_INVALIDARG;
    if (QueryGetData(fmt) != S_OK) return DV_E_FORMATETC;
    const size_t bytes = sizeof(DROPFILES) + (path_.size() + 2) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!h) return E_OUTOFMEMORY;
    auto* mem = static_cast<BYTE*>(GlobalLock(h));
    if (!mem) { GlobalFree(h); return E_OUTOFMEMORY; }
    auto* drop = reinterpret_cast<DROPFILES*>(mem);
    drop->pFiles = sizeof(DROPFILES);
    drop->fWide = TRUE;
    std::memcpy(mem + sizeof(DROPFILES), path_.c_str(), (path_.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(h);
    med->tymed = TYMED_HGLOBAL; med->hGlobal = h; med->pUnkForRelease = nullptr;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return DATA_E_FORMATETC; }
  HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fmt) override {
    return fmt && fmt->cfFormat == CF_HDROP && (fmt->tymed & TYMED_HGLOBAL) && fmt->dwAspect == DVASPECT_CONTENT ? S_OK : DV_E_FORMATETC;
  }
  HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override { if (out) out->ptd = nullptr; return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
  HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
  HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};
class FileDragSource final : public IDropSource {
  LONG refs_ = 1;
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid == IID_IUnknown || iid == IID_IDropSource) { *out = static_cast<IDropSource*>(this); AddRef(); return S_OK; }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs_); }
  ULONG STDMETHODCALLTYPE Release() override { auto n = InterlockedDecrement(&refs_); if (!n) delete this; return n; }
  HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
    if (escape) return DRAGDROP_S_CANCEL;
    if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};
inline bool DragFileToHost(const std::wstring& path) {
  const HRESULT init = OleInitialize(nullptr);
  if (FAILED(init)) return false;
  auto* data = new FileDragDataObject(path);
  auto* source = new FileDragSource();
  DWORD effect = DROPEFFECT_NONE;
  const HRESULT hr = DoDragDrop(data, source, DROPEFFECT_COPY, &effect);
  data->Release(); source->Release();
  OleUninitialize();
  return hr == DRAGDROP_S_DROP && effect == DROPEFFECT_COPY;
}
} // namespace rvrse
#endif
