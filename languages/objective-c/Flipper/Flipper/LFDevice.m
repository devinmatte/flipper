//
//  LFDevice.m
//  Flipper
//
//  Created by George Morgan on 12/27/16.
//  Copyright © 2016 Flipper. All rights reserved.
//

#import "LFDevice.h"
#define __private_include__
#import "flipper.h"

@implementation LFDevice

- (id) initWithName:(NSString *)name {
    if (self == [super init]) {
        [self setLed:[[LFLED alloc] init]];
        flipper_attach_usb([name UTF8String]);
        return self;
    }
    return NULL;
}

@end
