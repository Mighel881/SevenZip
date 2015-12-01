//
//  SVZForwardingStream.mm
//  SevenZip
//
//  Created by Tamas Lustyik on 2015. 11. 26..
//  Copyright © 2015. Tamas Lustyik. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "SVZForwardingStream.h"

namespace SVZ {

    STDMETHODIMP ForwardingStream::Read(void* data, UInt32 size, UInt32* processedSize) {
        @autoreleasepool {
            NSInteger bytesRead = [_source read:(uint8_t*)data maxLength:size];
            if (bytesRead < 0) {
                // stream error
                if (processedSize) {
                    *processedSize = 0;
                }
                return E_FAIL;
            }
            
            if (processedSize) {
                *processedSize = (UInt32)bytesRead;
            }
        }

        return S_OK;
    }
    
    ForwardingStream::ForwardingStream(NSInputStream* source): _source(source) {
        @autoreleasepool {
            [_source open];
        }
    }
    
    ForwardingStream::~ForwardingStream() {
        @autoreleasepool {
            [_source close];
        }
        _source = nil;
    }
}
