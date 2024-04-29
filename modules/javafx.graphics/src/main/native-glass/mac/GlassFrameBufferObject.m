/*
 * Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#import "GlassFrameBufferObject.h"
#import "GlassMacros.h"
#import "GlassApplication.h"

//#define VERBOSE
#ifndef VERBOSE
    #define LOG(MSG, ...)
#else
    #define LOG(MSG, ...) GLASS_LOG(MSG, ## __VA_ARGS__);
#endif

@implementation GlassFrameBufferObject

- (void)_destroyFbo
{
    if (self->_texture != 0)
    {
        // TODO: MTL: this is owned by prism in case of PresentingPainter - hence do not release it.
        // In case of UploadingPainter, this will probably leak. Fix is needed.
        // [self->_texture release]; 
        self->_texture = 0;
    }
}

- (void)_createFboIfNeededForWidth:(unsigned int)width andHeight:(unsigned int)height
{
    if ((self->_width != width) || (self->_height != height))
    {
        // TODO optimization: is it possible to just resize an FBO's texture without destroying it first?
        [self _destroyFbo];
    }

    if (self->_texture == nil) {
        // Create a texture ----------
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();

        MTLTextureDescriptor *texDescriptor =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatBGRA8Unorm
                                                                 width:width
                                                                height:height
                                                             mipmapped:false];

        texDescriptor.usage = MTLTextureUsageRenderTarget;

        self->_texture = [device newTextureWithDescriptor:texDescriptor];
    }

    self->_width = width;
    self->_height = height;
}

- (id)init
{
    self = [super init];
    if (self != nil)
    {
        self->_width = 0;
        self->_height = 0;
        self->_texture = 0;
        //self->_fbo = 0;
        self->_isSwPipe = NO;
    }
    return self;
}

- (void)dealloc
{
   //[self _assertContext];
    {
        [self _destroyFbo];
    }

    [super dealloc];
}

- (GLuint)width
{
    return self->_width;
}

- (GLuint)height
{
    return self->_height;
}

- (void)bindForWidth:(unsigned int)width andHeight:(unsigned int)height
{
    LOG("           GlassFrameBufferObject bindForWidth:%d andHeight:%d", width, height);
    {
        if ((width > 0) && (height > 0))
        {
            if(self->_isSwPipe)
            {
                // self->_fboToRestore = 0; // default to screen
                // glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, (GLint*)&self->_fboToRestore);
                // LOG("               will need to restore to FBO: %d", self->_fboToRestore);
            }

            [self _createFboIfNeededForWidth:width andHeight:height];
        }
    }
}

- (void)unbind
{
    //TODO: MTL:
}

- (void)blitForWidth:(unsigned int)width andHeight:(unsigned int)height
{
    //TODO: MTL:
}

- (void)blitFromFBO:(GlassFrameBufferObject*)other_fbo
{
    [self _createFboIfNeededForWidth:other_fbo->_width andHeight:other_fbo->_height];
}


- (id<MTLTexture>)texture
{
    return self->_texture;
}

/*- (GLuint)fbo
{
    return self->_fbo;
}*/

- (void)setIsSwPipe:(BOOL)isSwPipe
{
    self->_isSwPipe = isSwPipe;
}

@end
