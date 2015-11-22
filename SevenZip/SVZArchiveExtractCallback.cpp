//
//  SVZArchiveExtractCallback.cpp
//  ObjC7z
//
//  Created by Tamas Lustyik on 2015. 11. 21..
//  Copyright © 2015. Tamas Lustyik. All rights reserved.
//

#include "SVZArchiveExtractCallback.h"

#include "CPP/Windows/Defs.h"
#include "CPP/Windows/FileDir.h"
#include "CPP/Windows/FileFind.h"
#include "CPP/Windows/FileName.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/Windows/PropVariantConv.h"

#include "CPP/Common/UTFConvert.h"

#include "SVZOutFileStream.h"

static const wchar_t *kEmptyFileAlias = L"[Content]";

static HRESULT IsArchiveItemProp(IInArchive *archive, UInt32 index, PROPID propID, bool &result)
{
    NWindows::NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, propID, &prop));
    if (prop.vt == VT_BOOL)
        result = VARIANT_BOOLToBool(prop.boolVal);
    else if (prop.vt == VT_EMPTY)
        result = false;
    else
        return E_FAIL;
    return S_OK;
}

static HRESULT IsArchiveItemFolder(IInArchive *archive, UInt32 index, bool &result) {
    return IsArchiveItemProp(archive, index, kpidIsDir, result);
}

namespace SVZ {
    
    void ArchiveExtractCallback::Init(IInArchive *archiveHandler, const FString &directoryPath) {
        NumErrors = 0;
        _archiveHandler = archiveHandler;
        _directoryPath = directoryPath;
        NWindows::NFile::NName::NormalizeDirPathPrefix(_directoryPath);
    }
    
    STDMETHODIMP ArchiveExtractCallback::SetTotal(UInt64 /* size */) {
        return S_OK;
    }
    
    STDMETHODIMP ArchiveExtractCallback::SetCompleted(const UInt64 * /* completeValue */) {
        return S_OK;
    }
    
    STDMETHODIMP ArchiveExtractCallback::GetStream(UInt32 index,
                                                   ISequentialOutStream **outStream,
                                                   Int32 askExtractMode) {
        *outStream = 0;
        _outFileStream.Release();
        
        {
            // Get Name
            NWindows::NCOM::CPropVariant prop;
            RINOK(_archiveHandler->GetProperty(index, kpidPath, &prop));
            
            UString fullPath;
            if (prop.vt == VT_EMPTY)
                fullPath = kEmptyFileAlias;
            else
            {
                if (prop.vt != VT_BSTR)
                    return E_FAIL;
                fullPath = prop.bstrVal;
            }
            _filePath = fullPath;
        }
        
        if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
            return S_OK;
        
        {
            // Get Attrib
            NWindows::NCOM::CPropVariant prop;
            RINOK(_archiveHandler->GetProperty(index, kpidAttrib, &prop));
            if (prop.vt == VT_EMPTY)
            {
                _processedFileInfo.Attrib = 0;
                _processedFileInfo.AttribDefined = false;
            }
            else
            {
                if (prop.vt != VT_UI4)
                    return E_FAIL;
                _processedFileInfo.Attrib = prop.ulVal;
                _processedFileInfo.AttribDefined = true;
            }
        }
        
        RINOK(IsArchiveItemFolder(_archiveHandler, index, _processedFileInfo.isDir));
        
        {
            // Get Modified Time
            NWindows::NCOM::CPropVariant prop;
            RINOK(_archiveHandler->GetProperty(index, kpidMTime, &prop));
            _processedFileInfo.MTimeDefined = false;
            switch (prop.vt)
            {
                case VT_EMPTY:
                    // _processedFileInfo.MTime = _utcMTimeDefault;
                    break;
                case VT_FILETIME:
                    _processedFileInfo.MTime = prop.filetime;
                    _processedFileInfo.MTimeDefined = true;
                    break;
                default:
                    return E_FAIL;
            }
            
        }
        {
            // Get Size
            NWindows::NCOM::CPropVariant prop;
            RINOK(_archiveHandler->GetProperty(index, kpidSize, &prop));
            UInt64 newFileSize;
            /* bool newFileSizeDefined = */ ConvertPropVariantToUInt64(prop, newFileSize);
        }
        
        
        {
            // Create folders for file
            int slashPos = _filePath.ReverseFind_PathSepar();
            if (slashPos >= 0) {
                NWindows::NFile::NDir::CreateComplexDir(_directoryPath + us2fs(_filePath.Left(slashPos)));
            }
        }
        
        FString fullProcessedPath = _directoryPath + us2fs(_filePath);
        _diskFilePath = fullProcessedPath;
        
        if (_processedFileInfo.isDir)
        {
            NWindows::NFile::NDir::CreateComplexDir(fullProcessedPath);
        }
        else
        {
            NWindows::NFile::NFind::CFileInfo fi;
            if (fi.Find(fullProcessedPath))
            {
                if (!NWindows::NFile::NDir::DeleteFileAlways(fullProcessedPath))
                {
//                    PrintError("Can not delete output file", fullProcessedPath);
                    return E_ABORT;
                }
            }
            
            _outFileStreamImpl = new OutFileStream();
            CMyComPtr<ISequentialOutStream> outStreamLoc(_outFileStreamImpl);
            AString utf8Path;
            ConvertUnicodeToUTF8(fs2us(fullProcessedPath), utf8Path);
            if (!_outFileStreamImpl->Open(utf8Path.Ptr()))
            {
//                PrintError("Can not open output file", fullProcessedPath);
                return E_ABORT;
            }
            _outFileStream = outStreamLoc;
            *outStream = outStreamLoc.Detach();
        }
        return S_OK;
    }
    
    STDMETHODIMP ArchiveExtractCallback::PrepareOperation(Int32 askExtractMode) {
        _extractMode = false;
        switch (askExtractMode)
        {
            case NArchive::NExtract::NAskMode::kExtract:  _extractMode = true; break;
        }
        return S_OK;
    }
    
    STDMETHODIMP ArchiveExtractCallback::SetOperationResult(Int32 operationResult) {
        switch (operationResult)
        {
            case NArchive::NExtract::NOperationResult::kOK:
                break;
            default:
            {
                NumErrors++;
            }
        }
        
        if (_outFileStream)
        {
//            if (_processedFileInfo.MTimeDefined) {
//                _outFileStreamImpl->SetMTime(&_processedFileInfo.MTime);
//            }
            _outFileStreamImpl->Close();
        }
        _outFileStream.Release();
        if (_extractMode && _processedFileInfo.AttribDefined)
            NWindows::NFile::NDir::SetFileAttrib(_diskFilePath, _processedFileInfo.Attrib);
        return S_OK;
    }
    
    
    STDMETHODIMP ArchiveExtractCallback::CryptoGetTextPassword(BSTR *password) {
        if (!PasswordIsDefined) {
            // You can ask real password here from user
            // Password = GetPassword(OutStream);
            // PasswordIsDefined = true;
//            PrintError("Password is not defined");
            return E_ABORT;
        }
        return StringToBstr(Password, password);
    }
    
}