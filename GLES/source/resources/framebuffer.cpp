/**
 * Copyright (C) 2015-2018 Think Silicon S.A. (https://think-silicon.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public v3
 * License as published by the Free Software Foundation;
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */

/**
 *  @file       framebuffer.cpp
 *  @author     Think Silicon
 *  @date       25/07/2018
 *  @version    1.0
 *
 *  @brief      Framebuffer Functionality in GLOVE
 *
 *  @scope
 *
 *  Framebuffer Objects allow the creation of user-defined framebuffers for
 *  offscreen rendering.
 *
 */

#include "framebuffer.h"
#include "utils/VkToGlConverter.h"
#include "utils/glUtils.h"
#include "context/context.h"

Framebuffer::Framebuffer(const vulkanAPI::vkContext_t *vkContext)
: mVkContext(vkContext),
mTarget(GL_INVALID_VALUE), mState(IDLE),
mUpdated(true), mSizeUpdated(false), mDepthStencilTexture(nullptr),
mBindToTexture(false), mSurfaceType(GLOVE_SURFACE_INVALID),
mIsSystem(false), mEGLSurfaceInterface(nullptr),
mCacheColorTexture(nullptr), mCacheDepthTexture(nullptr), mCacheStencilTexture(nullptr),
mCacheColorRenderbuffer(nullptr), mCacheDepthRenderbuffer(nullptr), mCacheStencilRenderbuffer(nullptr)
{
    FUN_ENTRY(GL_LOG_TRACE);

    mRenderPass           = new vulkanAPI::RenderPass(vkContext);
    mAttachmentDepth      = new Attachment();
    mAttachmentStencil    = new Attachment();
}

Framebuffer::~Framebuffer()
{
    FUN_ENTRY(GL_LOG_TRACE);

    delete mRenderPass;
    delete mAttachmentDepth;
    delete mAttachmentStencil;

    if(!mIsSystem && mDepthStencilTexture != nullptr) {
        if(mDepthStencilTexture->GetDepthStencilTextureRefCount() == 1) {
            delete mDepthStencilTexture;
        } else {
            mDepthStencilTexture->DecreaseDepthStencilTextureRefCount();
        }
        mDepthStencilTexture = nullptr;
    }

    for(auto color : mAttachmentColors) {
        if(color) {
            delete color;
            color = nullptr;
        }
    }
    mAttachmentColors.clear();

    Release();
}

void
Framebuffer::Release(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    for(auto fb : mFramebuffers) {
        delete fb;
        fb = nullptr;
    }
    mFramebuffers.clear();
}

size_t
Framebuffer::GetCurrentBufferIndex() const
{
    return mIsSystem ? mEGLSurfaceInterface->nextImageIndex : 0;
}

Texture *
Framebuffer::GetColorAttachmentTexture(void) const
{
    FUN_ENTRY(GL_LOG_TRACE);

    if(mIsSystem) {
        size_t bufferIndex = GetCurrentBufferIndex();
        return mAttachmentColors[bufferIndex]->GetTexture();
    }

    Texture * tex   = nullptr;
    uint32_t  index = GetColorAttachmentName();

    if(index) {
        GLenum    type  = GetColorAttachmentType();
        if(type == GL_TEXTURE) {
            if(mCacheColorTexture) {
                tex = mCacheColorTexture;
            } else {
                tex = mTextureArray->GetObject(index);
            }
        } else if(type == GL_RENDERBUFFER) {
            if(mCacheColorRenderbuffer) {
                tex = mCacheColorRenderbuffer->GetTexture();
            } else {
                tex = mRenderbufferArray->GetObject(index)->GetTexture();
            }
        }
    }

    return tex;
}

Texture *
Framebuffer::GetDepthAttachmentTexture(void) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mIsSystem) {
        return mDepthStencilTexture;
    }

    Texture * tex   = nullptr;
    uint32_t  index = GetDepthAttachmentName();

    if(index) {
        GLenum    type  = GetDepthAttachmentType();
        if(type == GL_TEXTURE) {
            if(mCacheDepthTexture) {
                tex = mCacheDepthTexture;
            } else {
                tex = mTextureArray->GetObject(index);
            }
        } else if(type == GL_RENDERBUFFER) {
            if(mCacheDepthRenderbuffer) {
                tex = mCacheDepthRenderbuffer->GetTexture();
            } else {
                tex = mRenderbufferArray->GetObject(index)->GetTexture();
            }
        }
    }

    return tex;
}

Texture *
Framebuffer::GetStencilAttachmentTexture(void) const
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mIsSystem) {
        return mDepthStencilTexture;
    }

    Texture * tex   = nullptr;
    uint32_t  index = GetStencilAttachmentName();

    if(index) {
        GLenum    type  = GetStencilAttachmentType();
        if(type == GL_TEXTURE) {
            if(mCacheStencilTexture) {
                tex = mCacheStencilTexture;
            } else {
                tex = mTextureArray->GetObject(index);
            }
        } else if(type == GL_RENDERBUFFER) {
            if(mCacheStencilRenderbuffer) {
                tex = mCacheStencilRenderbuffer->GetTexture();
            } else {
                tex = mRenderbufferArray->GetObject(index)->GetTexture();
            }
        }
    }

    return tex;
}

void
Framebuffer::AddColorAttachment(Texture *texture)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    Attachment *att = new Attachment(texture);
    mAttachmentColors.push_back(att);

    SetWidth(texture->GetWidth());
    SetHeight(texture->GetHeight());

    mUpdated = true;
}

void
Framebuffer::SetColorAttachment(int width, int height)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mAttachmentColors.size() == 0) {
        Attachment *att = new Attachment();
        mAttachmentColors.push_back(att);
    }

    SetWidth(width);
    SetHeight(height);


    mUpdated     = true;
    mSizeUpdated = (mDepthStencilTexture == nullptr) || (mDepthStencilTexture->GetWidth() != GetWidth() || mDepthStencilTexture->GetHeight() != GetHeight());
}

GLenum
Framebuffer::CheckStatus(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(GetColorAttachmentType()    == GL_NONE  &&
       GetDepthAttachmentType()    == GL_NONE  &&
       GetStencilAttachmentType()  == GL_NONE) {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    if((GetColorAttachmentType() != GL_NONE   &&
        (!GlFormatIsColorRenderable(GetColorAttachmentTexture()->GetInternalFormat()) ||
         GetColorAttachmentTexture()->GetWidth()  <= 0   ||
         GetColorAttachmentTexture()->GetHeight() <= 0   )
        ) ||
       (GetDepthAttachmentType() != GL_NONE    &&
        (!GlFormatIsDepthRenderable(GetDepthAttachmentTexture()->GetInternalFormat())  ||
         GetDepthAttachmentTexture()->GetWidth()  <= 0 ||
         GetDepthAttachmentTexture()->GetHeight() <= 0)
       ) ||
       (GetStencilAttachmentType() != GL_NONE  &&
        (!GlFormatIsStencilRenderable(GetStencilAttachmentTexture()->GetInternalFormat())  ||
         GetStencilAttachmentTexture()->GetWidth()  <= 0 ||
         GetStencilAttachmentTexture()->GetHeight() <= 0))
       ) {
        return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    }

    if(GetColorAttachmentType() != GL_NONE && GetDepthAttachmentType() != GL_NONE) {
        if(GetColorAttachmentTexture()->GetWidth()  != GetDepthAttachmentTexture()->GetWidth() ||
           GetColorAttachmentTexture()->GetHeight() != GetDepthAttachmentTexture()->GetHeight()) {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if(GetColorAttachmentType() != GL_NONE && GetStencilAttachmentType() != GL_NONE) {
        if(GetColorAttachmentTexture()->GetWidth()  != GetStencilAttachmentTexture()->GetWidth() ||
           GetColorAttachmentTexture()->GetHeight() != GetStencilAttachmentTexture()->GetHeight()) {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if(GetDepthAttachmentType() != GL_NONE && GetStencilAttachmentType() != GL_NONE) {
        if(GetDepthAttachmentTexture()->GetWidth()  != GetStencilAttachmentTexture()->GetWidth() ||
           GetDepthAttachmentTexture()->GetHeight() != GetStencilAttachmentTexture()->GetHeight()) {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

bool
Framebuffer::CreateVkRenderPass(bool clearColorEnabled, bool clearDepthEnabled, bool clearStencilEnabled,
                                bool writeColorEnabled, bool writeDepthEnabled, bool writeStencilEnabled)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    mRenderPass->SetColorClearEnabled(clearColorEnabled);
    mRenderPass->SetDepthClearEnabled(clearDepthEnabled);
    mRenderPass->SetStencilClearEnabled(clearStencilEnabled);

    mRenderPass->SetColorWriteEnabled(writeColorEnabled);
    mRenderPass->SetDepthWriteEnabled(writeDepthEnabled);
    mRenderPass->SetStencilWriteEnabled(writeStencilEnabled);

    return mRenderPass->Create(GetColorAttachmentTexture() ?
                               GetColorAttachmentTexture()->GetVkFormat() : VK_FORMAT_UNDEFINED,
                               mDepthStencilTexture ?
                               mDepthStencilTexture->GetVkFormat() : VK_FORMAT_UNDEFINED);
}

void
Framebuffer::CreateDepthStencilTexture(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(GetDepthAttachmentTexture() || GetStencilAttachmentTexture()) {
       
        if(!mIsSystem && GetDepthAttachmentTexture() && GetDepthAttachmentTexture()->GetDepthStencilTexture()) {
           mDepthStencilTexture = GetDepthAttachmentTexture()->GetDepthStencilTexture();
           mDepthStencilTexture->IncreaseDepthStencilTextureRefCount();
           return;
        }

        if(mDepthStencilTexture != nullptr) {
            delete mDepthStencilTexture;
            mDepthStencilTexture = nullptr;
        }
        
        mDepthStencilTexture = new Texture(mVkContext);
        mDepthStencilTexture->SetTarget(GL_TEXTURE_2D);

        VkFormat vkformat = GlInternalFormatToVkFormat(
            GetDepthAttachmentTexture()   ? GetDepthAttachmentTexture()->GetInternalFormat()   : GL_INVALID_VALUE,
            GetStencilAttachmentTexture() ? GetStencilAttachmentTexture()->GetInternalFormat() : GL_INVALID_VALUE);

        // convert to supported format
        vkformat = FindSupportedDepthStencilFormat(mVkContext->vkGpus[0], GetVkFormatDepthBits(vkformat), GetVkFormatStencilBits(vkformat));
        mDepthStencilTexture->SetVkFormat(vkformat);
        mDepthStencilTexture->SetVkImageUsage(static_cast<VkImageUsageFlagBits>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
        mDepthStencilTexture->SetVkImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        mDepthStencilTexture->SetVkImageTiling();
        GLenum glformat = VkFormatToGlInternalformat(mDepthStencilTexture->GetVkFormat());
        mDepthStencilTexture->InitState();
        mDepthStencilTexture->SetState(GetWidth(), GetHeight(), 0, 0, GlInternalFormatToGlFormat(glformat),
                                       GlInternalFormatToGlType(glformat), Texture::GetDefaultInternalAlignment(), nullptr);
        mDepthStencilTexture->Allocate();

        if(!mIsSystem && GetDepthAttachmentTexture()) {
            GetDepthAttachmentTexture()->SetDepthStencilTexture(mDepthStencilTexture);
            mDepthStencilTexture->IncreaseDepthStencilTextureRefCount();
        }
    }
}

void
Framebuffer::UpdateClearDepthStencilTexture(uint32_t clearStencil, uint32_t stencilMaskFront, const Rect& clearRect)
{
    GLenum glFormat = VkFormatToGlInternalformat(mDepthStencilTexture->GetVkFormat());
    size_t numElements = GlInternalFormatTypeToNumElements(glFormat, mDepthStencilTexture->GetExplicitType());
    ImageRect srcRect(clearRect, numElements, 1, Texture::GetDefaultInternalAlignment());

    ImageRect dstRect = srcRect;
    dstRect.x = 0;
    dstRect.y = 0;
    size_t dstByteSize = dstRect.GetRectBufferSize();
    uint8_t* dstData = new uint8_t[dstByteSize];

    //retrieve the stored stencil data
    mDepthStencilTexture->SetDataNoInvertion(true);
    mDepthStencilTexture->SetImageBufferCopyStencil(true);
    mDepthStencilTexture->CopyPixelsToHost(&srcRect, &dstRect, 0, 0, glFormat, dstData);

    // size of an entire row in bytes
    uint32_t dataRowStride = dstRect.GetRectAlignedRowInBytes();

    // rectangle offset in the memory block
    uint32_t dataCurrentRowIndex = dstRect.GetStartRowIndex(dataRowStride);

    // obtain ptr locations with the byte offset
    uint8_t* dataPtr = static_cast<uint8_t*>(dstData) + dataCurrentRowIndex;

    GLint depthBits = 0;
    GLint stencilBits = 0;
    GlFormatToStorageBits(glFormat, nullptr, nullptr, nullptr, nullptr, &depthBits, &stencilBits);
    assert(stencilBits > 0);

    // perform the conversion
    for(int row = 0; row < srcRect.height; ++row) {
        for(int col = 0; col < srcRect.width; ++col) {
            uint32_t dataIndex = col * srcRect.GetPixelByteOffset();
            uint8_t oldStencilValue = dataPtr[dataIndex];
            uint8_t newStencilValue = (clearStencil & 0xFF) | ((oldStencilValue & 0xFF) & (~(stencilMaskFront & 0xFF)));

            dataPtr[dataIndex] = newStencilValue;
        }
        // offset by the number of bytes per row
        dataPtr = dataPtr + dataRowStride;
    }

    //update the stencil data
    mDepthStencilTexture->CopyPixelsFromHost(&dstRect, &srcRect, 0, 0, glFormat, dstData);
    mDepthStencilTexture->SetImageBufferCopyStencil(false);

    delete[] dstData;
}

void
Framebuffer::CheckForUpdatedResources()
{
    if(GetColorAttachmentTexture()) {

        mUpdated |= GetColorAttachmentTexture()->GetDataUpdated();
        if( GetColorAttachmentTexture()->GetWidth()  != GetWidth()  ||
            GetColorAttachmentTexture()->GetHeight() != GetHeight() ) {

            SetWidth (GetColorAttachmentTexture()->GetWidth());
            SetHeight(GetColorAttachmentTexture()->GetHeight());

            mSizeUpdated = true;
        }
        GetColorAttachmentTexture()->SetDataUpdated(false);
    }
}

void
Framebuffer::CacheAttachement(Texture *bTexture, GLuint index)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(index == GetColorAttachmentName() && GL_TEXTURE == GetColorAttachmentType() && !mCacheColorTexture) {
        mCacheColorTexture = bTexture;
    }
    if(index == GetDepthAttachmentName() && GL_TEXTURE == GetDepthAttachmentType() && !mCacheDepthTexture) {
        mCacheDepthTexture = bTexture;
    }
    if(index == GetStencilAttachmentName() && GL_TEXTURE == GetStencilAttachmentType() && !mCacheStencilTexture) {
        mCacheStencilTexture = bTexture;
    }
}

void
Framebuffer::CacheAttachement(Renderbuffer *bRenderbuffer, GLuint index)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(index == GetColorAttachmentName() && GL_RENDERBUFFER == GetColorAttachmentType() && !mCacheColorRenderbuffer) {
        mCacheColorRenderbuffer = bRenderbuffer;
    }

    if(index == GetDepthAttachmentName() && GL_RENDERBUFFER == GetDepthAttachmentType() && !mCacheDepthRenderbuffer) {
        mCacheDepthRenderbuffer = bRenderbuffer;
    }

    if(index == GetStencilAttachmentName() && GL_RENDERBUFFER == GetStencilAttachmentType() && !mCacheStencilRenderbuffer) {
        mCacheStencilRenderbuffer = bRenderbuffer;
    }
}

void
Framebuffer::CleanCachedAttachment(GLenum attachment)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    switch(attachment) {
    case GL_COLOR_ATTACHMENT0: {
        uint32_t index = GetColorAttachmentName();
        if(index) {
            GLenum type  = GetColorAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheColorTexture) {
                    mCacheColorTexture = nullptr;
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheColorRenderbuffer) {
                    mCacheColorRenderbuffer = nullptr;
                }
            }
        }
        break; }
    case GL_DEPTH_ATTACHMENT: {
        uint32_t index = GetDepthAttachmentName();
        if(index) {
            GLenum type  = GetDepthAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheDepthTexture) {
                    mCacheDepthTexture = nullptr;
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheDepthRenderbuffer) {
                    mCacheDepthRenderbuffer = nullptr;
                }
            }
        }
        break; }
    case GL_STENCIL_ATTACHMENT: {
        uint32_t index = GetStencilAttachmentName();
        if(index) {
            GLenum type  = GetStencilAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheStencilTexture) {
                    mCacheStencilTexture = nullptr;
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheStencilRenderbuffer) {
                    mCacheStencilRenderbuffer = nullptr;
                }
            }
        }
        break; }
    }
}

void
Framebuffer::UnrefAttachment(GLenum attachment)
{
    FUN_ENTRY(GL_LOG_TRACE);

    switch(attachment) {
    case GL_COLOR_ATTACHMENT0: {
        uint32_t index = GetColorAttachmentName();
        if(index) {
            GLenum type  = GetColorAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheColorTexture) {
                    mCacheColorTexture->Unbind();
                } else {
                    mTextureArray->GetObject(index)->Unbind();
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheColorRenderbuffer) {
                    mCacheColorRenderbuffer->Unbind();
                } else {
                    mRenderbufferArray->GetObject(index)->Unbind();
                }
            }
        }
        break; }
    case GL_DEPTH_ATTACHMENT: {
        uint32_t index = GetDepthAttachmentName();
        if(index) {
            GLenum type  = GetDepthAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheDepthTexture) {
                    mCacheDepthTexture->Unbind();
                } else {
                    mTextureArray->GetObject(index)->Unbind();
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheDepthRenderbuffer) {
                    mCacheDepthRenderbuffer->Unbind();
                } else {
                    mRenderbufferArray->GetObject(index)->Unbind();
                }
            }
        }
        break; }
    case GL_STENCIL_ATTACHMENT: {
        uint32_t index = GetStencilAttachmentName();
        if(index) {
            GLenum type  = GetStencilAttachmentType();
            if(type == GL_TEXTURE) {
                if(mCacheStencilTexture) {
                    mCacheStencilTexture->Unbind();
                } else {
                    mTextureArray->GetObject(index)->Unbind();
                }
            } else if(type == GL_RENDERBUFFER) {
                if(mCacheStencilRenderbuffer) {
                    mCacheStencilRenderbuffer->Unbind();
                } else {
                    mRenderbufferArray->GetObject(index)->Unbind();
                }
            }
        }
        break; }
    }
}

void
Framebuffer::RefAttachment(GLenum attachment)
{
    FUN_ENTRY(GL_LOG_TRACE);

    switch(attachment) {
    case GL_COLOR_ATTACHMENT0: {
        uint32_t index = GetColorAttachmentName();
        if(index) {
            GLenum type  = GetColorAttachmentType();
            if(type == GL_TEXTURE) {
                mTextureArray->GetObject(index)->Bind();
            } else if(type == GL_RENDERBUFFER) {
                mRenderbufferArray->GetObject(index)->Bind();
            }
        }
        break; }
    case GL_DEPTH_ATTACHMENT: {
        uint32_t index = GetDepthAttachmentName();
        if(index) {
            GLenum type  = GetDepthAttachmentType();
            if(type == GL_TEXTURE) {
                mTextureArray->GetObject(index)->Bind();
            } else if(type == GL_RENDERBUFFER) {
                mRenderbufferArray->GetObject(index)->Bind();
            }
        }
        break; }
    case GL_STENCIL_ATTACHMENT: {
        uint32_t index = GetStencilAttachmentName();
        if(index) {
            GLenum type  = GetStencilAttachmentType();
            if(type == GL_TEXTURE) {
                mTextureArray->GetObject(index)->Bind();
            } else if(type == GL_RENDERBUFFER) {
                mRenderbufferArray->GetObject(index)->Bind();
            }
        }
        break; }
    }
}

VkFramebuffer*
Framebuffer::GetActiveVkFramebuffer() const
{
    FUN_ENTRY(GL_LOG_TRACE);

    size_t bufferIndex = mIsSystem ? mEGLSurfaceInterface->nextImageIndex : 0;
    return mFramebuffers[bufferIndex]->GetFramebuffer();
}

void
Framebuffer::CreateRenderPass(bool clearColorEnabled, bool clearDepthEnabled, bool clearStencilEnabled,
                               bool writeColorEnabled, bool writeDepthEnabled, bool writeStencilEnabled,
                               const float *colorValue, float depthValue, uint32_t stencilValue,
                               const Rect  *clearRect)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(mUpdated || mSizeUpdated ||
       static_cast<bool>(mRenderPass->GetColorClearEnabled())   != clearColorEnabled    ||
       static_cast<bool>(mRenderPass->GetDepthClearEnabled())   != clearDepthEnabled    ||
       static_cast<bool>(mRenderPass->GetStencilClearEnabled()) != clearStencilEnabled  ||
       static_cast<bool>(mRenderPass->GetColorWriteEnabled())   != writeColorEnabled    ||
       static_cast<bool>(mRenderPass->GetDepthWriteEnabled())   != writeDepthEnabled    ||
       static_cast<bool>(mRenderPass->GetStencilWriteEnabled()) != writeStencilEnabled) {

        if(!mIsSystem && mSizeUpdated) {
            CreateDepthStencilTexture();
            mSizeUpdated = false;
        }
        CreateVkRenderPass(clearColorEnabled, clearDepthEnabled, clearStencilEnabled,
                           writeColorEnabled, writeDepthEnabled, writeStencilEnabled);
        Create();

        mUpdated = false;
    }


    const VkRect2D clearRect2D = { {clearRect->x, clearRect->y},
                                   {(uint32_t)clearRect->width, (uint32_t)clearRect->height}};

    mRenderPass->SetClearArea(&clearRect2D);
    mRenderPass->SetClearColorValue(colorValue);
    mRenderPass->SetClearDepthStencilValue(depthValue, stencilValue);
}

void
Framebuffer::BeginVkRenderPass()
{
    FUN_ENTRY(GL_LOG_DEBUG);

    assert(GetCurrentContext());
    vulkanAPI::CommandBufferManager *commandBufferManager = GetCurrentContext()->GetVkCommandBufferManager();
    VkCommandBuffer activeCmdBuffer = commandBufferManager->GetActiveCommandBuffer();
    size_t bufferIndex = GetCurrentBufferIndex();
    mRenderPass->Begin(&activeCmdBuffer, mFramebuffers[bufferIndex]->GetFramebuffer(), true);
}

bool
Framebuffer::EndVkRenderPass(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    assert(GetCurrentContext());
    vulkanAPI::CommandBufferManager *commandBufferManager = GetCurrentContext()->GetVkCommandBufferManager();
    VkCommandBuffer activeCmdBuffer = commandBufferManager->GetActiveCommandBuffer();
    return mRenderPass->End(&activeCmdBuffer);
}

void
Framebuffer::PrepareVkImage(VkImageLayout newImageLayout)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    if(GetColorAttachmentTexture() && newImageLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        GetColorAttachmentTexture()->PrepareVkImageLayout(newImageLayout);
    } else if(GetDepthStencilAttachmentTexture() && newImageLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        GetDepthStencilAttachmentTexture()->PrepareVkImageLayout(newImageLayout);
    }
}

bool
Framebuffer::Create(void)
{
    FUN_ENTRY(GL_LOG_DEBUG);

    Release();

    for(uint32_t i = 0; i < mAttachmentColors.size(); ++i) {
        vulkanAPI::Framebuffer *frameBuffer = new vulkanAPI::Framebuffer(mVkContext);

        vector<VkImageView> imageViews;
        if(GetColorAttachmentTexture(i)) {
            imageViews.push_back(GetColorAttachmentTexture(i)->GetVkImageView());
        }
        if(mDepthStencilTexture) {
            imageViews.push_back(mDepthStencilTexture->GetVkImageView());
        }

        if(!frameBuffer->Create(&imageViews, GetVkRenderPass(), GetWidth(), GetHeight())) {
            delete frameBuffer;
            return false;
        }

        mFramebuffers.push_back(frameBuffer);
    }

    return true;
}
