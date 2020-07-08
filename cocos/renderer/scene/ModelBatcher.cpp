/****************************************************************************
 Copyright (c) 2018 Xiamen Yaji Software Co., Ltd.

 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "ModelBatcher.hpp"
#include "RenderFlow.hpp"
#include "StencilManager.hpp"
#include "assembler/RenderDataList.hpp"
#include "NodeProxy.hpp"
#include "assembler/AssemblerSprite.hpp"

RENDERER_BEGIN

#define OPTIMIZE_BATCH 1
#define INIT_MODEL_LENGTH 16

ModelBatcher::ModelBatcher(RenderFlow* flow)
: _flow(flow)
, _modelOffset(0)
, _cullingMask(0)
, _walking(false)
, _currEffect(nullptr)
, _buffer(nullptr)
, _useModel(false)
, _customProps(nullptr)
, _node(nullptr)
{
    for (int i = 0; i < INIT_MODEL_LENGTH; i++)
    {
        _modelPool.push_back(new Model());
    }

    _stencilMgr = StencilManager::getInstance();
}

ModelBatcher::~ModelBatcher()
{
    setCurrentEffect(nullptr);
    setNode(nullptr);
    
    for (int i = 0; i < _modelPool.size(); i++)
    {
        auto model = _modelPool[i];
        delete model;
    }
    _modelPool.clear();
    
    for (auto iter = _buffers.begin(); iter != _buffers.end(); ++iter)
    {
        MeshBuffer *buffer = iter->second;
        delete buffer;
    }
    _buffers.clear();
}

void ModelBatcher::reset()
{
    for (int i = 0; i < _modelOffset; ++i)
    {
        Model* model = _modelPool[i];
        model->reset();
    }
    _flow->getRenderScene()->removeModels();
    _modelOffset = 0;
    
    for (auto iter : _buffers)
    {
        iter.second->reset();
    }
    _buffer = nullptr;
    
    _commitState = CommitState::None;
    setCurrentEffect(nullptr);
    setNode(nullptr);
    _ia.clear();
    _cullingMask = 0;
    _walking = false;
    _useModel = false;
    
    _modelMat.set(Mat4::IDENTITY);
    _stencilMgr->reset();
    _drawCmdList.clear();
}

void ModelBatcher::changeCommitState(CommitState state)
{
    if (_commitState == state) return;
    switch(_commitState)
    {
        case CommitState::Custom:
            flushIA();
            break;
        case CommitState::Common:
            flush();
            break;
        default:
            break;
    }
    setCurrentEffect(nullptr);
    setCustomProperties(nullptr);
    _commitState = state;
}

void ModelBatcher::commit(NodeProxy* node, Assembler* assembler, int cullingMask)
{
#if OPTIMIZE_BATCH
    DrawCmdInfo cmdInfo;
    cmdInfo.assembler = assembler;
    cmdInfo.cullingMask = cullingMask;
    cmdInfo.node = node;
    cmdInfo.customAssembler = nullptr;
    _drawCmdList.push_back(cmdInfo);
#else
    _commit(node, assembler, cullingMask);
#endif
}

void ModelBatcher::_commit(NodeProxy* node, Assembler* assembler, int cullingMask)
{
    changeCommitState(CommitState::Common);
    
    VertexFormat* vfmt = assembler->getVertexFormat();
    if (!vfmt)
    {
        return;
    }
    
    bool useModel = assembler->getUseModel();
    bool ignoreWorldMatrix = assembler->isIgnoreWorldMatrix();
    const Mat4& nodeWorldMat = node->getWorldMatrix();
    const Mat4& worldMat = useModel && !ignoreWorldMatrix ? nodeWorldMat : Mat4::IDENTITY;
    
    auto asmDirty = assembler->isDirty(AssemblerBase::VERTICES_OPACITY_CHANGED);
    auto nodeDirty = node->isDirty(RenderFlow::NODE_OPACITY_CHANGED);
    auto needUpdateOpacity = (asmDirty || nodeDirty) && !assembler->isIgnoreOpacityFlag();
    
    for (std::size_t i = 0, l = assembler->getIACount(); i < l; ++i)
    {
        assembler->beforeFillBuffers(i);
        
        Effect* effect = assembler->getEffect(i);
        CustomProperties* customProp = assembler->getCustomProperties();
        if (!effect) continue;

        if (_currEffect == nullptr ||
            _currEffect->getHash() != effect->getHash() ||
            _cullingMask != cullingMask || useModel)
        {
            // Break auto batch
            flush();
            
            setNode(_useModel ? node : nullptr);
            setCurrentEffect(effect);
            setCustomProperties(customProp);
            _modelMat.set(worldMat);
            _useModel = useModel;
            _cullingMask = cullingMask;
        }
        
        if (needUpdateOpacity)
        {
            assembler->updateOpacity(i, node->getRealOpacity());
        }
        
        MeshBuffer* buffer = _buffer;
        if (!_buffer || vfmt != _buffer->_vertexFmt)
        {
            buffer = getBuffer(vfmt);
        }
        assembler->fillBuffers(node, buffer, i);
    }
}

void ModelBatcher::commitIA(NodeProxy* node, CustomAssembler* assembler, int cullingMask)
{
#if OPTIMIZE_BATCH
    DrawCmdInfo cmdInfo;
    cmdInfo.assembler = nullptr;
    cmdInfo.cullingMask = cullingMask;
    cmdInfo.node = node;
    cmdInfo.customAssembler = assembler;
    _drawCmdList.push_back(cmdInfo);
#else
    _commitIA(node, assembler, cullingMask);
#endif
}

void ModelBatcher::_commitIA(NodeProxy* node, CustomAssembler* assembler, int cullingMask)
{
    changeCommitState(CommitState::Custom);

    Effect* effect = assembler->getEffect(0);
    if (!effect) return;

    auto customIA = assembler->getIA(0);
    if (!customIA) return;
    
    std::size_t iaCount = assembler->getIACount();
    bool useModel = assembler->getUseModel();
    const Mat4& worldMat = useModel ? node->getWorldMatrix() : Mat4::IDENTITY;
    
    if (_currEffect == nullptr ||
    _currEffect->getHash() != effect->getHash() ||
    _cullingMask != cullingMask || useModel ||
    !_ia.isMergeable(*customIA))
    {
        flushIA();
        
        setNode(_useModel ? node : nullptr);
        setCurrentEffect(effect);
        _modelMat.set(worldMat);
        _useModel = useModel;
        _cullingMask = cullingMask;
        
        _ia.setVertexBuffer(customIA->getVertexBuffer());
        _ia.setIndexBuffer(customIA->getIndexBuffer());
        _ia.setStart(customIA->getStart());
        _ia.setCount(0);
    }
    
    for (std::size_t i = 0; i < iaCount; i++ )
    {
        customIA = assembler->getIA(i);
        effect = assembler->getEffect(i);
        if (!effect) continue;
        
        if (i > 0)
        {
            flushIA();
            
            setNode(_useModel ? node : nullptr);
            setCurrentEffect(effect);
            _modelMat.set(worldMat);
            _useModel = useModel;
            _cullingMask = cullingMask;
            
            _ia.setVertexBuffer(customIA->getVertexBuffer());
            _ia.setIndexBuffer(customIA->getIndexBuffer());
            _ia.setStart(customIA->getStart());
            _ia.setCount(0);
        }
        
        _ia.setCount(_ia.getCount() + customIA->getCount());
    }
}

void ModelBatcher::flushIA()
{
    _flush();
    if (_commitState != CommitState::Custom)
    {
        return;
    }
    
    if (!_walking || !_currEffect || _ia.getCount() <= 0)
    {
        _ia.clear();
        return;
    }
    
    // Stencil manager process
    _stencilMgr->handleEffect(_currEffect);
    
    // Generate model
    Model* model = nullptr;
    if (_modelOffset >= _modelPool.size())
    {
        model = new Model();
        _modelPool.push_back(model);
    }
    else
    {
        model = _modelPool[_modelOffset];
    }
    _modelOffset++;
    model->setWorldMatix(_modelMat);
    model->setCullingMask(_cullingMask);
    model->setEffect(_currEffect, _customProps);
    model->setNode(_node);
    model->setInputAssembler(_ia);
    
    _ia.clear();
    
    _flow->getRenderScene()->addModel(model);
}

void ModelBatcher::flush()
{
    _flush();
    if (_commitState != CommitState::Common)
    {
        return;
    }
    
    if (!_walking || !_currEffect || !_buffer)
    {
        return;
    }
    
    int indexStart = _buffer->getIndexStart();
    int indexOffset = _buffer->getIndexOffset();
    int indexCount = indexOffset - indexStart;
    if (indexCount <= 0)
    {
        return;
    }
    
    _ia.setVertexBuffer(_buffer->getVertexBuffer());
    _ia.setIndexBuffer(_buffer->getIndexBuffer());
    _ia.setStart(indexStart);
    _ia.setCount(indexCount);
    
    // Stencil manager process
    _stencilMgr->handleEffect(_currEffect);
    
    // Generate model
    Model* model = nullptr;
    if (_modelOffset >= _modelPool.size())
    {
        model = new Model();
        _modelPool.push_back(model);
    }
    else
    {
        model = _modelPool[_modelOffset];
    }
    _modelOffset++;
    model->setWorldMatix(_modelMat);
    model->setCullingMask(_cullingMask);
    model->setEffect(_currEffect, _customProps);
    model->setNode(_node);
    model->setInputAssembler(_ia);
    
    _ia.clear();

    _flow->getRenderScene()->addModel(model);
    
    _buffer->updateOffset();
}
 
void ModelBatcher::_flush()
{
#if OPTIMIZE_BATCH
    for (auto i = 0; i < _drawCmdList.size(); i++)
    {
        auto node = _drawCmdList[i].node;

        double hash = 0;
        auto assembler = dynamic_cast<Assembler *>(node->getAssembler());
        auto assemblerSprite = dynamic_cast<AssemblerSprite *>(node->getAssembler());
        cocos2d::Rect aabb{};
        // 计算绘制的顶点AABB
        if (assembler && assembler->_datas && assembler->_datas->getMeshCount())
        {
            auto renderData = assembler->_datas->getRenderData(0);
            float *vertices = (float *)renderData->getVertices();
            uint32_t vertexCount = (uint32_t)renderData->getVBytes() / assembler->_bytesPerVertex;
            float minX = vertices[0];
            float maxX = minX;
            float minY = vertices[1];
            float maxY = minY;
            for (auto v = 0; v < vertexCount; v++, vertices = (float *)(((uint8_t *)vertices) + assembler->_bytesPerVertex))
            {
                float x = vertices[0];
                float y = vertices[1];
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }

            auto min = cocos2d::Vec3(minX, minY, 0);
            auto max = cocos2d::Vec3(maxX, maxY, 0);
            //AssemblerSprite的顶点是已经算好了世界坐标的点了的
            if (!assemblerSprite)
            {
                node->getWorldMatrix().transformPoint(&min);
                node->getWorldMatrix().transformPoint(&max);
            }
            aabb.setRect(min.x, min.y, max.x - min.x, max.y - min.y);
            auto effect = assembler->getEffect(0);
            if(effect) {
                hash = effect->getHash();
            }
        }
        _drawCmdList[i].aabb = aabb;
        _drawCmdList[i].effectHash = hash;
    }

    //优化排列的绘制列表 更利于batch
    _drawCmdListTemp.clear();
    for (auto &drawCmdInfo : _drawCmdList)
    {
        auto index = _drawCmdListTemp.size();
        for (auto i = _drawCmdListTemp.size(); i > 0; i--)
        {
            auto &node = _drawCmdListTemp[i - 1];
            if (node.aabb.intersectsRect(drawCmdInfo.aabb))
            {
                break;
            }
            if (node.effectHash == drawCmdInfo.effectHash)
            {
                index = i;
                break;
            }
        }
        _drawCmdListTemp.insert(_drawCmdListTemp.begin() + index, drawCmdInfo);
    }

    _drawCmdList.clear();

    for (auto &drawCmdInfo : _drawCmdListTemp)
    {
        if (drawCmdInfo.assembler)
        {
            _commit(drawCmdInfo.node, drawCmdInfo.assembler, drawCmdInfo.cullingMask);
        }
        else if (drawCmdInfo.customAssembler)
        {
            _commitIA(drawCmdInfo.node, drawCmdInfo.customAssembler, drawCmdInfo.cullingMask);
        }
    }
    _drawCmdListTemp.clear();
#endif
}

void ModelBatcher::startBatch()
{
    reset();
    _walking = true;
}

void ModelBatcher::terminateBatch()
{
    flush();
    flushIA();
    
    for (auto iter : _buffers)
    {
        iter.second->uploadData();
    }
    
    _walking = false;
}

void ModelBatcher::setNode(NodeProxy* node)
{
    if (_node == node)
    {
        return;
    }
    CC_SAFE_RELEASE(_node);
    _node = node;
    CC_SAFE_RETAIN(_node);
}

void ModelBatcher::setCurrentEffect(Effect* effect)
{
    if (_currEffect == effect)
    {
        return;
    }
    CC_SAFE_RELEASE(_currEffect);
    _currEffect = effect;
    CC_SAFE_RETAIN(_currEffect);
};

MeshBuffer* ModelBatcher::getBuffer(VertexFormat* fmt)
{
    MeshBuffer* buffer = nullptr;
    auto iter = _buffers.find(fmt);
    if (iter == _buffers.end())
    {
        buffer = new MeshBuffer(this, fmt);
        _buffers.emplace(fmt, buffer);
    }
    else
    {
        buffer = iter->second;
    }
    return buffer;
}

RENDERER_END
