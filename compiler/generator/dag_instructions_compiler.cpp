/************************************************************************
 ************************************************************************
    FAUST compiler
	Copyright (C) 2003-2004 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/

/**********************************************************************
			- code_gen.h : generic code generator (projet FAUST) -


		Historique :
		-----------

***********************************************************************/

#include "dag_instructions_compiler.hh"

#include "ppsig.hh"
#include "Text.hh"
#include "sigtyperules.hh"
#include "timing.hh"
#include "recursivness.hh"
#include "privatise.hh"
#include "simplify.hh"
#include "xtended.hh"
#include "prim2.hh"

extern int gMaxCopyDelay;

void DAGInstructionsCompiler::compileMultiSignal(Tree L)
{
	L = prepare(L);		// Optimize, share and annotate expression
    
    // "input" and "inputs" used as a name convention
    for (int index = 0; index < fContainer->inputs(); index++) {
        string name1 = subst("fInput$0_ptr", T(index));
        string name2 = subst("fInput$0", T(index));
        
        //if (!gVectorSwitch) {
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(name1, InstBuilder::genArrayTyped(InstBuilder::genBasicTyped(Typed::kFloatMacro), 0), Address::kStruct));
        fContainer->pushComputeBlockMethod(InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(name1, Address::kStruct), 
                InstBuilder::genLoadVarInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress("inputs", Address::kFunArgs), InstBuilder::genIntNumInst(index)))));
                            
                            
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(name2, InstBuilder::genArrayTyped(InstBuilder::genBasicTyped(Typed::kFloatMacro), 0), Address::kStruct));
                        
        /*
        } else {
             fContainer->pushComputeBlockMethod(
                InstBuilder::genDeclareVarInst(name, InstBuilder::genArrayTyped(InstBuilder::genVectorTyped(InstBuilder::genBasicTyped(xfloat()), gVecSize), 0), Address::kStack,  
                    InstBuilder::genLoadVarInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress("inputs", Address::kFunArgs), InstBuilder::genIntNumInst(index)))));
        }
        */
    }
    
    // "output" and "outputs" used as a name convention
    for (int index = 0; index < fContainer->outputs(); index++) {
        string name1 = subst("fOutput$0_ptr", T(index));
        string name2 = subst("fOutput$0", T(index));
        
        // if (!gVectorSwitch) {
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(name1, InstBuilder::genArrayTyped(InstBuilder::genBasicTyped(Typed::kFloatMacro), 0), Address::kStruct));                
        fContainer->pushComputeBlockMethod(InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(name1, Address::kStruct), 
                InstBuilder::genLoadVarInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress("outputs", Address::kFunArgs), InstBuilder::genIntNumInst(index)))));
                            
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(name2, InstBuilder::genArrayTyped(InstBuilder::genBasicTyped(Typed::kFloatMacro), 0), Address::kStruct));
                            
        /*                
        } else {
             fContainer->pushComputeBlockMethod(
                InstBuilder::genDeclareVarInst(name, InstBuilder::genArrayTyped(InstBuilder::genVectorTyped(InstBuilder::genBasicTyped(xfloat()), gVecSize), 0), Address::kStack,  
                    InstBuilder::genLoadVarInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress("outputs", Address::kFunArgs), InstBuilder::genIntNumInst(index)))));
        }
        */
    }
    
	for (int index = 0; isList(L); L = tl(L), index++) {
		Tree sig = hd(L);
        string name = subst("fOutput$0", T(index));
        
        fContainer->openLoop();
        
        // Cast to external float
        ValueInst* res = CS(kSamp, sig);
        res = InstBuilder::genCastNumInst(CS(kSamp, sig), InstBuilder::genBasicTyped(Typed::kFloatMacro));
        
        fContainer->getCurLoop()->pushComputeDSPMethod(
            InstBuilder::genStoreVarInst(
                InstBuilder::genIndexedAddress(
                    InstBuilder::genNamedAddress(name, Address::kStruct), 
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))), res));  
                        
        fContainer->closeLoop();
    }
    
	generateUserInterfaceTree(prepareUserInterfaceTree(fUIRoot));
	generateMacroInterfaceTree("", prepareUserInterfaceTree(fUIRoot));
	if (fDescription) {
		fDescription->ui(prepareUserInterfaceTree(fUIRoot));
	}
}

ValueInst* DAGInstructionsCompiler::CS(int variability, Tree sig)
{
    int i;
    Tree x;
    ValueInst* code;
    if (!getCompiledExpression(sig, code)) {
        code = generateCode(variability, sig);
        setCompiledExpression(sig, code);
    } else {
        // check for recursive dependencies
        CodeLoop* ls;
        CodeLoop* tl = fContainer->getCurLoop();
        if (isProj(sig, &i, x) && tl->findRecDefinition(x)) {
            tl->addRecDependency(x);
		} else if (fContainer->getLoopProperty(sig, ls)) {
			tl->addBackwardDependency(ls);
		} else {
			Tree x, d;
			if (isSigFixDelay(sig, x, d)) {
				if (fContainer->getLoopProperty(x, ls)) {
					tl->addBackwardDependency(ls);
				}
			}
        }
    }
	return code;
}

ValueInst* DAGInstructionsCompiler::generateVariableStore(Tree sig, ValueInst* exp)
{
    ::Type t = getSigType(sig);

    if (getSigType(sig)->variability() == kSamp) {
        string vname;
        Typed::VarType ctype;
        getTypedNames(t, "Vector", ctype, vname);
        Address::AccessType var_access;
        generateVectorLoop(ctype, vname, exp, var_access);
        return InstBuilder::genLoadVarInst(
                    InstBuilder::genIndexedAddress(
                        InstBuilder::genNamedAddress(vname, var_access), 
                            InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));
    } else {
        return InstructionsCompiler::generateVariableStore(sig, exp);
    }
}

ValueInst* DAGInstructionsCompiler::generateInput(int variability, Tree sig, int idx) 
{
    // "input" use as a name convention
    string name = subst("fInput$0", T(idx));
    ValueInst* res = InstBuilder::genLoadVarInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress(name, Address::kStruct), 
                                InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)))); 
    // Cast to internal float
    res = InstBuilder::genCastNumInst(res, InstBuilder::genBasicTyped(itfloat()));
    return generateCacheCode(sig, res);
}

ValueInst* DAGInstructionsCompiler::generateCacheCode(Tree sig, ValueInst* exp)
{
    string      vname;
    Typed::VarType    ctype;
    int         sharing = getSharingCount(sig);
    Type        t = getSigType(sig);
    Occurences* o = fOccMarkup.retrieve(sig);
    int         d = o->getMaxDelay();

    if (t->variability() < kSamp) {
        if (d == 0) {
            // non-sample, not delayed : same as scalar cache
            return InstructionsCompiler::generateCacheCode(sig, exp);

        } else {
            // it is a non-sample expressions but used delayed
            // we need a delay line
			getTypedNames(getSigType(sig), "Vec", ctype, vname);
            Address::AccessType var_access;
            
            if ((sharing > 1) && !verySimple(sig)) {
                // first cache this expression because it
                // it is shared and complex
                ValueInst* cachedexp =  generateVariableStore(sig, exp);
                generateDelayLine(cachedexp, ctype, vname, d, var_access);
                setVectorNameProperty(sig, vname);
                return cachedexp;
            } else {
                // no need to cache this expression because
                // it is either not shared or very simple
                generateDelayLine(exp, ctype, vname, d, var_access);
                setVectorNameProperty(sig, vname);
                return exp;
            }
        }
    } else {
        // sample-rate signal
        if (d > 0) {
            // used delayed : we need a delay line
            getTypedNames(getSigType(sig), "Yec", ctype, vname);
            Address::AccessType var_access;
            generateDelayLine(exp, ctype, vname, d, var_access);
            setVectorNameProperty(sig, vname);

            if (verySimple(sig)) {
                return exp;
            } else {
                // return subst("$0[i]", vname);
                return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress(vname, var_access), 
                                InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));
            }
        } else {
            // not delayed
            if (sharing > 1 && !verySimple(sig)) {
                // shared and not simple : we need a vector
                // cerr << "ZEC : " << ppsig(sig) << endl;
                getTypedNames(getSigType(sig), "Zec", ctype, vname);
                Address::AccessType var_access;
                generateDelayLine(exp, ctype, vname, d, var_access);
                setVectorNameProperty(sig, vname);
                // return subst("$0[i]", vname);
                return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress(vname, var_access), 
                                InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));
           } else {
                // not shared or simple : no cache needed
                return exp;
            }
        }
    }
}

// Code generation

ValueInst* DAGInstructionsCompiler::generateCode(int variability, Tree sig)
{
    int         i;
    Tree        x;
    CodeLoop*   l;

    l = fContainer->getCurLoop();
    assert(l);

    if (needSeparateLoop(sig)) {
        // we need a separate loop unless it's an old recursion
        if (isProj(sig, &i, x)) {
            // projection of a recursive group x
            if (l->findRecDefinition(x)) {
                // x is already in the loop stack
                l->addRecDependency(x);
                return InstructionsCompiler::generateCode(variability, sig);
            } else {
                // x must be defined
                fContainer->openLoop(x);
                ValueInst* code = InstructionsCompiler::generateCode(variability, sig);
                fContainer->closeLoop(sig);
                return code;
            }
        } else {
            fContainer->openLoop();
            ValueInst* code = InstructionsCompiler::generateCode(variability, sig);
            fContainer->closeLoop(sig);
            return code;
        }
    } else {
        return InstructionsCompiler::generateCode(variability, sig);
    }
}

/**
 * Test if a signal need to be compiled in a separate loop.
 * @param sig the signal expression to test.
 * @return true if a separate loop is needed
 */
bool DAGInstructionsCompiler::needSeparateLoop(Tree sig)
{
    Occurences* o = fOccMarkup.retrieve(sig);
    Type        t = getSigType(sig);
    int         c = getSharingCount(sig);
    bool        b;

    int         i;
    Tree        x,y;

    if (o->getMaxDelay() > 0) {
        b = true;
    } else if (verySimple(sig) || t->variability()<kSamp) {
        b = false;      // non sample computation never require a loop
    } else if (isSigFixDelay(sig, x, y)) {
        b = false;      
    } else if (isProj(sig, &i, x)) {
        b = true;
    } else if (c > 1) {
        b = true;
    } else {
        // sample expressions that are not recursive, not delayed
        // and not shared, doesn't require a separate loop.
        b = false;
    }
    return b;
}

ValueInst* DAGInstructionsCompiler::generateFixDelay(int variability, Tree sig, Tree exp, Tree delay)
{
    int mxd, d; 
    string vname;
 
    CS(variability, exp); // ensure exp is compiled to have a vector name

    mxd = fOccMarkup.retrieve(exp)->getMaxDelay();

    if (!getVectorNameProperty(exp, vname)) {
        cerr << "no vector name for " << ppsig(exp) << endl;
        exit(1);
    }

    if (mxd == 0) {
        // not a real vector name but a scalar name
        //return subst("$0[i]", vname);
        return InstBuilder::genLoadVarInst(
                    InstBuilder::genIndexedAddress(
                        InstBuilder::genNamedAddress(vname, Address::kStack), 
                            InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));

    } else if (mxd < gMaxCopyDelay) {
        if (isSigInt(delay, &d)) {
            if (d == 0) {
                // return subst("$0[i]", vname);
                return InstBuilder::genLoadVarInst(
                    InstBuilder::genIndexedAddress(
                        InstBuilder::genNamedAddress(vname, Address::kStack), 
                            InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));
            } else {
                // return subst("$0[i-$1]", vname, T(d));
                ValueInst* index = InstBuilder::genBinopInst(kSub,
                                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                                            InstBuilder::genIntNumInst(d));
                return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                                                        InstBuilder::genNamedAddress(vname, Address::kStack), index));
            }
        } else {
            // return subst("$0[i-$1]", vname, CS(delay));
            ValueInst* index = InstBuilder::genBinopInst(kSub,
                                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                                        CS(variability, delay));
            return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                                                        InstBuilder::genNamedAddress(vname, Address::kStack), index));
        }
    } else {

        // long delay : we use a ring buffer of size 2^x
        int N = pow2limit(mxd + gVecSize);
        string vname_idx = vname + "_idx";

        if (isSigInt(delay, &d)) {
            if (d == 0) {
                //return subst("$0[($0_idx+i)&$1]", vname, T(N-1));
                ValueInst* index1 = InstBuilder::genBinopInst(kAdd,
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(vname_idx, Address::kStruct)));
                ValueInst* index2 = InstBuilder::genBinopInst(kAND, index1, InstBuilder::genIntNumInst(N-1));
                return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                                                        InstBuilder::genNamedAddress(vname, Address::kStruct), index2));
            } else {
                //return subst("$0[($0_idx+i-$2)&$1]", vname, T(N-1), T(d));
                ValueInst* index1 = InstBuilder::genBinopInst(kAdd,
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(vname_idx, Address::kStruct)));
                ValueInst* index2 = InstBuilder::genBinopInst(kSub, index1, InstBuilder::genIntNumInst(d));
                ValueInst* index3 = InstBuilder::genBinopInst(kAND, index2, InstBuilder::genIntNumInst(N-1));
                return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                                                        InstBuilder::genNamedAddress(vname, Address::kStruct), index3));
            }
        } else {
            //return subst("$0[($0_idx+i-$2)&$1]", vname, T(N-1), CS(delay));
            ValueInst* index1 = InstBuilder::genBinopInst(kAdd,
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(vname_idx, Address::kStruct)));
            ValueInst* index2 = InstBuilder::genBinopInst(kSub, index1, CS(variability, delay));
            ValueInst* index3 = InstBuilder::genBinopInst(kAND, index2, InstBuilder::genIntNumInst(N-1));
            return InstBuilder::genLoadVarInst(InstBuilder::genIndexedAddress(
                                                        InstBuilder::genNamedAddress(vname, Address::kStruct), index3));
        }
    }
}
   
ValueInst* DAGInstructionsCompiler::generateDelayVec(Tree sig, ValueInst* exp, Typed::VarType ctype, const string& vname, int mxd)
{
    // it is a non-sample but used delayed
    // we need a delay line
    Address::AccessType var_access;
    generateDelayLine(exp, ctype, vname, mxd, var_access);
    setVectorNameProperty(sig, vname);
    if (verySimple(sig)) {
        return exp;
    } else {
        return InstBuilder::genLoadVarInst(
                    InstBuilder::genIndexedAddress(
                        InstBuilder::genNamedAddress(vname, var_access), 
                            InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))));
    }
}

ValueInst* DAGInstructionsCompiler::generateDelayLine(ValueInst* exp, Typed::VarType ctype, const string& vname, int mxd, Address::AccessType& var_access)
{
    if (mxd == 0) {
        generateVectorLoop(ctype, vname, exp, var_access);
    } else {
        generateDlineLoop(ctype, vname, mxd, exp, var_access);
    }
    
    return exp;
}

void DAGInstructionsCompiler::generateVectorLoop(Typed::VarType ctype, const string& vname, ValueInst* exp, Address::AccessType& var_access)
{
    // -- declare the vector
    ValueInst* init;
    BasicTyped* typed = InstBuilder::genBasicTyped(ctype);
    
    if (ctype == Typed::kInt) {
        init = InstBuilder::genIntNumInst(0); 
    } else {  // Real type
        init = InstBuilder::genRealNumInst(ctype, 0); 
    }
    
    // "$0 $1[$2];"
    DeclareVarInst* table_inst = InstBuilder::genDeclareVarInst(vname, InstBuilder::genArrayTyped(typed, gVecSize), Address::kStack);
    fContainer->pushComputeBlockMethod(table_inst);
    
    // -- compute the new samples
    // $0[i] = $1;"
    fContainer->getCurLoop()->pushComputeDSPMethod(InstBuilder::genStoreVarInst(
                            InstBuilder::genIndexedAddress(
                                InstBuilder::genNamedAddress(vname, Address::kStack),
                                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))), exp));
    
    // Set desired variable access                              
    var_access = Address::kStack;
}

void DAGInstructionsCompiler::generateDlineLoop(Typed::VarType ctype, const string& vname, int delay, ValueInst* exp, Address::AccessType& var_access)
{
    ValueInst* init;
    BasicTyped* typed = InstBuilder::genBasicTyped(ctype);
    
    if (ctype == Typed::kInt) {
        init = InstBuilder::genIntNumInst(0); 
    } else {  // Real type
        init = InstBuilder::genRealNumInst(ctype, 0); 
    }
        
    if (delay < gMaxCopyDelay) {
        
        // Implementation of a copy based delayline
        // create names for temporary and permanent storage  
	    string buf = subst("$0_tmp", vname); 			
        string pmem = subst("$0_perm", vname);
        
        // constraints delay size to be multiple of 4
        delay = (delay+3)&-4;
        
        // allocate permanent storage for delayed samples
        fContainer->pushInitMethod(generateInitArray(pmem, ctype, delay));
        
        // compute method
        
        // -- declare a buffer and a "shifted" vector
        DeclareVarInst* table_inst1 = InstBuilder::genDeclareVarInst(buf, InstBuilder::genArrayTyped(typed, gVecSize + delay), Address::kStack);
        fContainer->pushComputeBlockMethod(table_inst1);
        
        ValueInst* address_value = InstBuilder::genLoadVarAddressInst(
                        InstBuilder::genIndexedAddress(
                            InstBuilder::genNamedAddress(buf, Address::kStack), InstBuilder::genIntNumInst(delay)));
        DeclareVarInst* table_inst2 = InstBuilder::genDeclareVarInst(vname, InstBuilder::genArrayTyped(typed, 0), Address::kStack, address_value);
        fContainer->pushComputeBlockMethod(table_inst2);
        
        // -- copy the stored samples to the delay line
        fContainer->getCurLoop()->pushComputePreDSPMethod(generateCopyArray(buf, pmem, delay));
        
        // -- compute the new samples
        fContainer->getCurLoop()->pushComputeDSPMethod(
            InstBuilder::genStoreVarInst(
                InstBuilder::genIndexedAddress(
                    InstBuilder::genNamedAddress(vname, Address::kStack), 
                        InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop))), exp));  
        
        // -- copy back to stored samples
        fContainer->getCurLoop()->pushComputePostDSPMethod(generateCopyBackArray(pmem, buf, delay));
        
        // Set desired variable access                              
        var_access = Address::kStack;
        
    } else {
        
        // Implementation of a ring-buffer delayline, the size should be large enough and aligned on a power of two
        delay = pow2limit(delay + gVecSize);
        
        // create names for temporary and permanent storage  
        string idx = subst("$0_idx", vname);
        string idx_save = subst("$0_idx_save", vname);
        
        // allocate permanent storage for delayed samples
        fContainer->pushInitMethod(generateInitArray(vname, ctype, delay));
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(idx, InstBuilder::genBasicTyped(Typed::kInt), Address::kStruct));
        fContainer->pushDeclare(InstBuilder::genDeclareVarInst(idx_save, InstBuilder::genBasicTyped(Typed::kInt), Address::kStruct));
        
        // init permanent memory
        fContainer->pushInitMethod(InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(idx, Address::kStruct), InstBuilder::genIntNumInst(0)));
        fContainer->pushInitMethod(InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(idx_save, Address::kStruct), InstBuilder::genIntNumInst(0)));
        
        // -- update index
        ValueInst* index1 = InstBuilder::genBinopInst(kAdd,
                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(idx, Address::kStruct)),
                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(idx_save, Address::kStruct)));
        ValueInst* index2 = InstBuilder::genBinopInst(kAND, index1, InstBuilder::genIntNumInst(delay-1));
    
        fContainer->getCurLoop()->pushComputePreDSPMethod(
            InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(idx, Address::kStruct), index2));
        
        // -- compute the new samples
        ValueInst* index3 = InstBuilder::genBinopInst(kAdd,
                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("i", Address::kLoop)),
                    InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress(idx, Address::kStruct)));
        ValueInst* index4 = InstBuilder::genBinopInst(kAND, index3, InstBuilder::genIntNumInst(delay-1));
        
        fContainer->getCurLoop()->pushComputeDSPMethod(
            InstBuilder::genStoreVarInst(
                InstBuilder::genIndexedAddress(
                    InstBuilder::genNamedAddress(vname, Address::kStruct), index4), exp));
        
        // -- save index
        fContainer->getCurLoop()->pushComputePostDSPMethod(
            InstBuilder::genStoreVarInst(InstBuilder::genNamedAddress(idx_save, Address::kStruct), 
                InstBuilder::genLoadVarInst(InstBuilder::genNamedAddress("count", Address::kStack))));
                
        // Set desired variable access                              
        var_access = Address::kStruct;
    }
}
