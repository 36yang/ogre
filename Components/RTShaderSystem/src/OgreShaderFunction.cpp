/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd
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
-----------------------------------------------------------------------------
*/

#include "OgreShaderPrecompiledHeaders.h"

namespace Ogre {
namespace RTShader {

static GpuConstantType typeFromContent(Parameter::Content content)
{
    switch (content)
    {
    case Parameter::SPC_COLOR_DIFFUSE:
    case Parameter::SPC_COLOR_SPECULAR:
    case Parameter::SPC_POSITION_PROJECTIVE_SPACE:
    case Parameter::SPC_POSITION_WORLD_SPACE:
    case Parameter::SPC_POSITION_OBJECT_SPACE:
        return GCT_FLOAT4;
    case Parameter::SPC_NORMAL_TANGENT_SPACE:
    case Parameter::SPC_NORMAL_OBJECT_SPACE:
    case Parameter::SPC_NORMAL_WORLD_SPACE:
        return GCT_FLOAT3;
    case Parameter::SPC_POINTSPRITE_SIZE:
        return GCT_FLOAT1;
    default:
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, "cannot derive type from content");
        break;
    }
}

void FunctionStageRef::callFunction(const char* name, const InOut& inout) const
{
    callFunction(name, std::vector<Operand>{inout});
}

void FunctionStageRef::callFunction(const char* name, const std::vector<Operand>& params) const
{
    auto function = new FunctionInvocation(name, mStage);
    function->setOperands(params);
    mParent->addAtomInstance(function);
}

void FunctionStageRef::sampleTexture(const std::vector<Operand>& params) const
{
    auto function = new SampleTextureAtom(mStage);
    function->setOperands(params);
    mParent->addAtomInstance(function);
}

void FunctionStageRef::assign(const std::vector<Operand>& params) const
{
    auto function = new AssignmentAtom(mStage);
    function->setOperands(params);
    mParent->addAtomInstance(function);
}

//-----------------------------------------------------------------------------
Function::Function(const String& name, const String& desc, const FunctionType functionType)
{
    mName           = name;
    mDescription    = desc;
    mFunctionType   = functionType;
}

//-----------------------------------------------------------------------------
Function::~Function()
{
    std::map<size_t, FunctionAtomInstanceList>::iterator jt;
    for(jt = mAtomInstances.begin(); jt != mAtomInstances.end(); ++jt)
    {
        for (FunctionAtomInstanceIterator it=jt->second.begin(); it != jt->second.end(); ++it)
            OGRE_DELETE (*it);
    }

    mAtomInstances.clear();

    for (ShaderParameterIterator it = mInputParameters.begin(); it != mInputParameters.end(); ++it)
        (*it).reset();
    mInputParameters.clear();

    for (ShaderParameterIterator it = mOutputParameters.begin(); it != mOutputParameters.end(); ++it)
        (*it).reset();
    mOutputParameters.clear();

    for (ShaderParameterIterator it = mLocalParameters.begin(); it != mLocalParameters.end(); ++it)
        (*it).reset();
    mLocalParameters.clear();

}

//-----------------------------------------------------------------------------
ParameterPtr Function::resolveInputParameter(Parameter::Semantic semantic,
                                        int index,
                                        const Parameter::Content content,
                                        GpuConstantType type)
{
    ParameterPtr param;

    // Check if desired parameter already defined.
    param = getParameterByContent(mInputParameters, content, type);
    if (param.get() != NULL)
        return param;

    // Case we have to create new parameter.
    if (index == -1)
    {
        index = 0;

        // Find the next available index of the target semantic.
        ShaderParameterIterator it;

        for (it = mInputParameters.begin(); it != mInputParameters.end(); ++it)
        {
            if ((*it)->getSemantic() == semantic)
            {
                index++;
            }
        }
    }
    else
    {
        // Check if desired parameter already defined.
        param = getParameterBySemantic(mInputParameters, semantic, index);
        if (param.get() != NULL && param->getContent() == content)
        {
            if (param->getType() == type)
            {
                return param;
            }
            else 
            {
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
                    "Can not resolve parameter - semantic: " + StringConverter::toString(semantic) + " - index: " + StringConverter::toString(index) + " due to type mismatch. Function <" + getName() + ">",           
                    "Function::resolveInputParameter" );
            }
        }
    }

    

    // No parameter found -> create new one.
    switch (semantic)
    {
    case Parameter::SPS_POSITION:   
        assert(type == GCT_FLOAT4);
        param = ParameterFactory::createInPosition(index);
        break;
            
    case Parameter::SPS_BLEND_WEIGHTS:          
        assert(type == GCT_FLOAT4);
        param = ParameterFactory::createInWeights(index);
        break;
            
    case Parameter::SPS_BLEND_INDICES:
		assert(type == GCT_FLOAT4);
        param = ParameterFactory::createInIndices(index);
        break;
            
    case Parameter::SPS_NORMAL:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createInNormal(index);
        break;
            
    case Parameter::SPS_COLOR:
        assert(type == GCT_FLOAT4);
        param = ParameterFactory::createInColor(index);
        break;
                        
    case Parameter::SPS_TEXTURE_COORDINATES:        
        param = ParameterFactory::createInTexcoord(type, index, content);               
        break;
            
    case Parameter::SPS_BINORMAL:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createInBiNormal(index);
        break;
            
    case Parameter::SPS_TANGENT:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createInTangent(index);
        break;
    case Parameter::SPS_UNKNOWN:
        break;
    }

    if (param.get() != NULL)
        addInputParameter(param);

    return param;
}

//-----------------------------------------------------------------------------
ParameterPtr Function::resolveOutputParameter(Parameter::Semantic semantic,
                                            int index,
                                            Parameter::Content content,                                         
                                            GpuConstantType type)
{
    ParameterPtr param;

    // Check if desired parameter already defined.
    param = getParameterByContent(mOutputParameters, content, type);
    if (param.get() != NULL)
        return param;

    // Case we have to create new parameter.
    if (index == -1)
    {
        index = 0;

        // Find the next available index of the target semantic.
        ShaderParameterIterator it;

        for (it = mOutputParameters.begin(); it != mOutputParameters.end(); ++it)
        {
            if ((*it)->getSemantic() == semantic)
            {
                index++;
            }
        }
    }
    else
    {
        // Check if desired parameter already defined.
        param = getParameterBySemantic(mOutputParameters, semantic, index);
        if (param.get() != NULL && param->getContent() == content)
        {
            if (param->getType() == type)
            {
                return param;
            }
            else 
            {
                OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
                    "Can not resolve parameter - semantic: " + StringConverter::toString(semantic) + " - index: " + StringConverter::toString(index) + " due to type mismatch. Function <" + getName() + ">",           
                    "Function::resolveOutputParameter" );
            }
        }
    }
    

    // No parameter found -> create new one.
    switch (semantic)
    {
    case Parameter::SPS_POSITION:   
        assert(type == GCT_FLOAT4);
        param = ParameterFactory::createOutPosition(index);
        break;

    case Parameter::SPS_BLEND_WEIGHTS:      
    case Parameter::SPS_BLEND_INDICES:
        OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
            "Can not resolve parameter - semantic: " + StringConverter::toString(semantic) + " - index: " + StringConverter::toString(index) + " since support in it is not implemented yet. Function <" + getName() + ">",             
            "Function::resolveOutputParameter" );
        break;

    case Parameter::SPS_NORMAL:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createOutNormal(index);
        break;

    case Parameter::SPS_COLOR:
        assert(type == GCT_FLOAT4);
        param = ParameterFactory::createOutColor(index);
        break;

    case Parameter::SPS_TEXTURE_COORDINATES:        
        param = ParameterFactory::createOutTexcoord(type, index, content);              
        break;

    case Parameter::SPS_BINORMAL:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createOutBiNormal(index);
        break;

    case Parameter::SPS_TANGENT:
        assert(type == GCT_FLOAT3);
        param = ParameterFactory::createOutTangent(index);
        break;
    case Parameter::SPS_UNKNOWN:
        break;
    }

    if (param.get() != NULL)
        addOutputParameter(param);

    return param;
}

//-----------------------------------------------------------------------------
ParameterPtr Function::resolveLocalParameter(Parameter::Semantic semantic, int index,
                                           const String& name,
                                           GpuConstantType type)
{
    ParameterPtr param;

    param = getParameterByName(mLocalParameters, name);
    if (param.get() != NULL)
    {
        if (param->getType() == type &&
            param->getSemantic() == semantic &&
            param->getIndex() == index)
        {
            return param;
        }
        else 
        {
            OGRE_EXCEPT(Exception::ERR_INVALIDPARAMS, 
                "Can not resolve local parameter due to type mismatch. Function <" + getName() + ">",           
                "Function::resolveLocalParameter" );
        }       
    }
        
    param = ParameterPtr(OGRE_NEW Parameter(type, name, semantic, index, Parameter::SPC_UNKNOWN));
    addParameter(mLocalParameters, param);
            
    return param;
}

//-----------------------------------------------------------------------------
ParameterPtr Function::resolveLocalParameter(Parameter::Semantic semantic, int index,
                                           const Parameter::Content content,
                                           GpuConstantType type)
{
    ParameterPtr param;

    if(type == GCT_UNKNOWN) type = typeFromContent(content);

    param = getParameterByContent(mLocalParameters, content, type);
    if (param.get() != NULL)    
        return param;

    param = ParameterPtr(OGRE_NEW Parameter(type, "lLocalParam_" + StringConverter::toString(mLocalParameters.size()), semantic, index, content));
    addParameter(mLocalParameters, param);

    return param;
}

//-----------------------------------------------------------------------------
void Function::addInputParameter(ParameterPtr parameter)
{

    // Check that parameter with the same semantic and index in input parameters list.
    if (getParameterBySemantic(mInputParameters, parameter->getSemantic(), parameter->getIndex()).get() != NULL)
    {
        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Parameter <" + parameter->getName() + "> has equal sematic parameter in function <" + getName() + ">",             
            "Function::addInputParameter" );
    }

    addParameter(mInputParameters, parameter);
}

//-----------------------------------------------------------------------------
void Function::addOutputParameter(ParameterPtr parameter)
{
    // Check that parameter with the same semantic and index in output parameters list.
    if (getParameterBySemantic(mOutputParameters, parameter->getSemantic(), parameter->getIndex()).get() != NULL)
    {
        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Parameter <" + parameter->getName() + "> has equal sematic parameter in function <" + getName() + ">",             
            "Function::addOutputParameter" );
    }

    addParameter(mOutputParameters, parameter);
}

//-----------------------------------------------------------------------------
void Function::deleteInputParameter(ParameterPtr parameter)
{
    deleteParameter(mInputParameters, parameter);
}

//-----------------------------------------------------------------------------
void Function::deleteOutputParameter(ParameterPtr parameter)
{
    deleteParameter(mOutputParameters, parameter);
}

//-----------------------------------------------------------------------------
void Function::deleteAllInputParameters()
{
    mInputParameters.clear();
}

//-----------------------------------------------------------------------------
void Function::deleteAllOutputParameters()
{
    mOutputParameters.clear();
}
//-----------------------------------------------------------------------------
void Function::addParameter(ShaderParameterList& parameterList, ParameterPtr parameter)
                                        
{
    // Check that parameter with the same name doest exist in input parameters list.
    if (getParameterByName(mInputParameters, parameter->getName()).get() != NULL)
    {
        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Parameter <" + parameter->getName() + "> already declared in function <" + getName() + ">",            
            "Function::addParameter" );
    }

    // Check that parameter with the same name doest exist in output parameters list.
    if (getParameterByName(mOutputParameters, parameter->getName()).get() != NULL)
    {
        OGRE_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Parameter <" + parameter->getName() + "> already declared in function <" + getName() + ">",            
            "Function::addParameter" );
    }


    // Add to given parameters list.
    parameterList.push_back(parameter);
}

//-----------------------------------------------------------------------------
void Function::deleteParameter(ShaderParameterList& parameterList, ParameterPtr parameter)
{
    ShaderParameterIterator it;

    for (it = parameterList.begin(); it != parameterList.end(); ++it)
    {
        if (*it == parameter)
        {
            (*it).reset();
            parameterList.erase(it);
            break;
        }
    }
}

//-----------------------------------------------------------------------------
ParameterPtr Function::getParameterByName( const ShaderParameterList& parameterList, const String& name )
{
    ShaderParameterConstIterator it;

    for (it = parameterList.begin(); it != parameterList.end(); ++it)
    {
        if ((*it)->getName() == name)
        {
            return *it;
        }
    }

    return ParameterPtr();
}

//-----------------------------------------------------------------------------
ParameterPtr Function::getParameterBySemantic(const ShaderParameterList& parameterList, 
                                                const Parameter::Semantic semantic, 
                                                int index)
{
    ShaderParameterConstIterator it;

    for (it = parameterList.begin(); it != parameterList.end(); ++it)
    {
        if ((*it)->getSemantic() == semantic &&
            (*it)->getIndex() == index)
        {
            return *it;
        }
    }

    return ParameterPtr();
}

//-----------------------------------------------------------------------------
ParameterPtr Function::getParameterByContent(const ShaderParameterList& parameterList, const Parameter::Content content, GpuConstantType type)
{
    ShaderParameterConstIterator it;

    if(type == GCT_UNKNOWN)
        type = typeFromContent(content);

    // Search only for known content.
    if (content != Parameter::SPC_UNKNOWN)  
    {
        for (it = parameterList.begin(); it != parameterList.end(); ++it)
        {
            if ((*it)->getContent() == content &&
                (*it)->getType() == type)
            {
                return *it;
            }
        }
    }
    
    return ParameterPtr();
}


//-----------------------------------------------------------------------------
void Function::addAtomInstance(FunctionAtom* atomInstance)
{
    mAtomInstances[atomInstance->getGroupExecutionOrder()].push_back(atomInstance);
    mSortedAtomInstances.clear();
}

//-----------------------------------------------------------------------------
void Function::addAtomAssign(ParameterPtr lhs, ParameterPtr rhs, int groupOrder)
{
    addAtomInstance(OGRE_NEW AssignmentAtom(lhs, rhs, groupOrder));
}

//-----------------------------------------------------------------------------
bool Function::deleteAtomInstance(FunctionAtom* atomInstance)
{
    FunctionAtomInstanceIterator it;
    size_t g = atomInstance->getGroupExecutionOrder();
    for (it=mAtomInstances[g].begin(); it != mAtomInstances[g].end(); ++it)
    {
        if (*it == atomInstance)
        {
            OGRE_DELETE atomInstance;
            mAtomInstances[g].erase(it);
            mSortedAtomInstances.clear();
            return true;
        }       
    }

    return false;
    
}

//-----------------------------------------------------------------------------
const FunctionAtomInstanceList& Function::getAtomInstances()
{
    if(!mSortedAtomInstances.empty())
        return mSortedAtomInstances;

    // put atom instances into order
    std::map<size_t, FunctionAtomInstanceList>::const_iterator it;
    for(it = mAtomInstances.begin(); it != mAtomInstances.end(); ++it)
    {
        mSortedAtomInstances.insert(mSortedAtomInstances.end(), it->second.begin(),
                                    it->second.end());
    }

    return mSortedAtomInstances;
}

//-----------------------------------------------------------------------------
Ogre::RTShader::Function::FunctionType Function::getFunctionType() const
{
    return mFunctionType;
}

}
}
