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

/*****************************************************************************
	HISTORY
	22/01/05 : corrected bug on bool signals cached in float variables
	2009-08-16 : First "doc" version (kb)
	2009-11-22 : Some clean up (kb)
*****************************************************************************/




#include <stdio.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>

#include "doc_compile.hh"
#include "sigtype.hh"
#include "floats.hh"
#include "sigprint.hh"
#include "sigtyperules.hh"
#include "recursivness.hh"
#include "simplify.hh"
#include "privatise.hh"
#include "prim2.hh"
#include "xtended.hh"
#include "compatibility.hh"
#include "ppsig.hh"
#include "names.hh"
#include "doc.hh"
#include "tlib.hh"
#include "doc_notice.hh"


extern bool		gLessTempSwitch;
extern int		gMaxCopyDelay;
extern map<string, string>		gDocMathStringMap;

extern bool		getSigListNickName(Tree t, Tree& id);

static void		extractMetadata(const string& fulllabel, string& label, map<string, set<string> >& metadata);
static string	rmWhiteSpaces(const string& s);


/*****************************************************************************
						getFreshID
*****************************************************************************/

map<string, int>	DocCompiler::fIDCounters;

string DocCompiler::getFreshID(const string& prefix)
{
	if (fIDCounters.find(prefix) == fIDCounters.end()) {
		fIDCounters[prefix] = 1;
	}
	int n = fIDCounters[prefix];
	fIDCounters[prefix] = n+1;
	
	return subst("$0_{$1}", prefix, docT(n));
}


/*****************************************************************************
						    prepare
*****************************************************************************/

Tree DocCompiler::annotate(Tree LS)
{
	recursivnessAnnotation(LS);		// Annotate LS with recursivness information
	typeAnnotation(LS);				// Annotate LS with type information
	sharingAnalysis(LS);			// annotate LS with sharing count
  	fOccMarkup.mark(LS);			// annotate LS with occurences analysis

  	return LS;
}

/*****************************************************************************
						    compileLateq
*****************************************************************************/

Lateq* DocCompiler::compileLateq (Tree L, Lateq* compiledEqn)
{
	//cerr << "Documentator : compileLateq : L = "; printSignal(L, stdout, 0); cerr << endl;
	
	fLateq = compiledEqn; ///< Dynamic field !
	int priority = 0;
	
	for (int i = 0; isList(L); L = tl(L), i++) {
		Tree sig = hd(L);
		Tree id;
		if(getSigNickname(sig, id)) {
			//cerr << "Documentator : compileLateq : NICKNAMEPROPERTY = " << tree2str(id) << endl;
			fLateq->addOutputSigFormula(subst("$0(t) = $1", tree2str(id), CS(sig, priority), docT(i)));	
		} else {
			//cerr << "Documentator : compileLateq : NO NICKNAMEPROPERTY" << endl;
			if (fLateq->outputs() == 1) {
				fLateq->addOutputSigFormula(subst("y(t) = $0", CS(sig, priority)));	
				gDocNoticeFlagMap["outputsig"] = true;
			} else {
				fLateq->addOutputSigFormula(subst("$0(t) = $1", getFreshID("y"), CS(sig, priority)));	
				gDocNoticeFlagMap["outputsigs"] = true;
			}
		}
	}
	return fLateq;
}



/*****************************************************************************
							 CS : compile a signal
*****************************************************************************/

/**
 * Test if a signal is already compiled
 * @param	sig		the signal expression to compile.
 * @param	name	the string representing the compiled expression.
 * @return	true	is already compiled
 */
bool DocCompiler::getCompiledExpression(Tree sig, string& cexp)
{
    return fCompileProperty.get(sig, cexp);
}


/**
 * Set the string of a compiled expression is already compiled
 * @param	sig		the signal expression to compile.
 * @param	cexp	the string representing the compiled expression.
 * @return	the cexp (for commodity)
 */
string DocCompiler::setCompiledExpression(Tree sig, const string& cexp)
{
    fCompileProperty.set(sig, cexp);
	return cexp;
}


/**
 * Compile a signal
 * @param sig the signal expression to compile.
 * @return the C code translation of sig as a string
 */
string  DocCompiler::CS (Tree sig, int priority)
{
    string      code;

    if (!getCompiledExpression(sig, code)) { // not compiled yet.
        code = generateCode(sig, priority);
        setCompiledExpression(sig, code);
    }
    return code;
}



/*****************************************************************************
					generateCode : dispatch according to signal
*****************************************************************************/


/**
 * @brief Main code generator dispatch.
 *
 * According to the type of the input signal, generateCode calls
 * the appropriate generator with appropriate arguments.
 *
 * @param	sig			The signal expression to compile.
 * @param	priority	The environment priority of the expression.
 * @return	<string>	The LaTeX code translation of the signal.
 */
string	DocCompiler::generateCode (Tree sig, int priority)
{
	int 	i;
	double	r;
	Tree 	c, sel, x, y, z, label, id, ff, largs, type, name, file;
	
	if ( getUserData(sig) )							{ printGCCall(sig,"generateXtended");	return generateXtended	(sig, priority);		}
	else if ( isSigInt(sig, &i) ) 					{ printGCCall(sig,"generateNumber");	return generateNumber	(sig, docT(i));			}
	else if ( isSigReal(sig, &r) ) 					{ printGCCall(sig,"generateNumber");	return generateNumber	(sig, docT(r));			}
	else if ( isSigInput(sig, &i) ) 				{ printGCCall(sig,"generateInput");		return generateInput	(sig, docT(i+1)); 		}
	else if ( isSigOutput(sig, &i, x) ) 			{ printGCCall(sig,"generateOutput");	return generateOutput	(sig, docT(i+1), CS(x, priority));	}
	
	else if ( isSigFixDelay(sig, x, y) ) 			{ printGCCall(sig,"generateFixDelay");	return generateFixDelay	(sig, x, y, priority); 	}
	else if ( isSigPrefix(sig, x, y) ) 				{ printGCCall(sig,"generatePrefix");	return generatePrefix	(sig, x, y, priority); 	}
	else if ( isSigIota(sig, x) ) 					{ printGCCall(sig,"generateIota");		return generateIota 	(sig, x); 				}
	
	else if ( isSigBinOp(sig, &i, x, y) )			{ printGCCall(sig,"generateBinOp");		return generateBinOp	(sig, i, x, y, priority); 		}
	else if ( isSigFFun(sig, ff, largs) )			{ printGCCall(sig,"generateFFun");		return generateFFun 	(sig, ff, largs, priority); 	}
    else if ( isSigFConst(sig, type, name, file) )  { printGCCall(sig,"generateFConst");	return generateFConst	(sig, tree2str(file), tree2str(name)); }
    else if ( isSigFVar(sig, type, name, file) )    { printGCCall(sig,"generateFVar");		return generateFVar		(sig, tree2str(file), tree2str(name)); }
	
	else if ( isSigTable(sig, id, x, y) ) 			{ printGCCall(sig,"generateTable");		return generateTable	(sig, x, y, priority);			}
	else if ( isSigWRTbl(sig, id, x, y, z) )		{ printGCCall(sig,"generateWRTbl");		return generateWRTbl	(sig, x, y, z, priority);		}
	else if ( isSigRDTbl(sig, x, y) ) 				{ printGCCall(sig,"generateRDTbl");		return generateRDTbl	(sig, x, y, priority);			}
	
	else if ( isSigSelect2(sig, sel, x, y) ) 		{ printGCCall(sig,"generateSelect2");	return generateSelect2 	(sig, sel, x, y, priority); 	}
	else if ( isSigSelect3(sig, sel, x, y, z) ) 	{ printGCCall(sig,"generateSelect3");	return generateSelect3 	(sig, sel, x, y, z, priority); 	}
	
	else if ( isSigGen(sig, x) ) 					{ printGCCall(sig,"generateSigGen");	return generateSigGen	(sig, x); 				}
	
    else if ( isProj(sig, &i, x) )                  { printGCCall(sig,"generateRecProj");	return generateRecProj	(sig, x, i, priority);	}
	
	else if ( isSigIntCast(sig, x) ) 				{ printGCCall(sig,"generateIntCast");	return generateIntCast	(sig, x, priority); 	}
	else if ( isSigFloatCast(sig, x) ) 				{ printGCCall(sig,"generateFloatCast");	return generateFloatCast(sig, x, priority); 	}
	
	else if ( isSigButton(sig, label) ) 			{ printGCCall(sig,"generateButton");	return generateButton   (sig, label); 			}
	else if ( isSigCheckbox(sig, label) ) 			{ printGCCall(sig,"generateCheckbox");	return generateCheckbox (sig, label); 			}
	else if ( isSigVSlider(sig, label,c,x,y,z) )	{ printGCCall(sig,"generateVSlider");	return generateVSlider 	(sig, label, c,x,y,z);	}
	else if ( isSigHSlider(sig, label,c,x,y,z) )	{ printGCCall(sig,"generateHSlider");	return generateHSlider 	(sig, label, c,x,y,z);	}
	else if ( isSigNumEntry(sig, label,c,x,y,z) )	{ printGCCall(sig,"generateNumEntry");	return generateNumEntry (sig, label, c,x,y,z);	}
	
	else if ( isSigVBargraph(sig, label,x,y,z) )	{ printGCCall(sig,"generateVBargraph");	return CS(z, priority);}//generateVBargraph 	(sig, label, x, y, CS(z, priority)); }
	else if ( isSigHBargraph(sig, label,x,y,z) )	{ printGCCall(sig,"generateHBargraph");	return CS(z, priority);}//generateHBargraph 	(sig, label, x, y, CS(z, priority)); }
	else if ( isSigAttach(sig, x, y) )				{ printGCCall(sig,"generateAttach");	return generateAttach	(sig, x, y, priority); }
	
	else {
		printf("Error in compiling signal, unrecognized signal : ");
		print(sig);
		printf("\n");
		exit(1);
	}
	return "error in generate code";
}


/**
 * Print calling information of generateCode, for debug purposes.
 *
 * @remark
 * To turn printing on, turn the 'printCalls' boolean to true.
 */
void DocCompiler::printGCCall(Tree sig, const string& calledFunction)
{
	bool printCalls	= false;
	bool maskSigs	= false;
	
	if(printCalls) {
		cerr << "  -> generateCode calls " << calledFunction;
		if(maskSigs) {
			cerr << endl;
		} else {
			cerr << " on " << ppsig(sig) << endl;
		}
	}
}


/*****************************************************************************
							   NUMBERS
*****************************************************************************/


string DocCompiler::generateNumber (Tree sig, const string& exp)
{
	string		ctype, vname;
	Occurences* o = fOccMarkup.retrieve(sig);

	// check for number occuring in delays
	if (o->getMaxDelay()>0) {
		getTypedNames(getSigType(sig), "r", ctype, vname);
		gDocNoticeFlagMap["recursigs"] = true;
		//cerr << "- r : generateNumber : \"" << vname << "\"" << endl;            
		generateDelayVec(sig, exp, ctype, vname, o->getMaxDelay());
	}
	return exp;
}

/*****************************************************************************
                               FOREIGN CONSTANTS
*****************************************************************************/


string DocCompiler::generateFConst (Tree sig, const string& file, const string& exp)
{
    string      ctype, vname;
    Occurences* o = fOccMarkup.retrieve(sig);

    if (o->getMaxDelay()>0) {
        getTypedNames(getSigType(sig), "r", ctype, vname);
		gDocNoticeFlagMap["recursigs"] = true;
		//cerr << "- r : generateFConst : \"" << vname << "\"" << endl;            
        generateDelayVec(sig, exp, ctype, vname, o->getMaxDelay());
    }
	
	if (exp == "fSamplingFreq") {
		gDocNoticeFlagMap["fsamp"] = true;
		return "f_S";
	}
	
    return "\\mathrm{"+exp+"}";
}

/*****************************************************************************
                               FOREIGN VARIABLES
*****************************************************************************/


string DocCompiler::generateFVar (Tree sig, const string& file, const string& exp)
{
    string      ctype, vname;
    Occurences* o = fOccMarkup.retrieve(sig);

    if (o->getMaxDelay()>0) {
        getTypedNames(getSigType(sig), "r", ctype, vname);
		gDocNoticeFlagMap["recursigs"] = true;
		//cerr << "- r : generateFVar : \"" << vname << "\"" << endl;            
		setVectorNameProperty(sig, vname);
        generateDelayVec(sig, exp, ctype, vname, o->getMaxDelay());
    }
    return generateCacheCode(sig, exp);
}


/*****************************************************************************
							   INPUTS - OUTPUTS
*****************************************************************************/


string DocCompiler::generateInput (Tree sig, const string& idx)
{
	if (fLateq->inputs() == 1) {
		setVectorNameProperty(sig, "x");
		fLateq->addInputSigFormula("x(t)");	
		gDocNoticeFlagMap["inputsig"] = true;
		return generateCacheCode(sig, "x(t)");
	} else {
		setVectorNameProperty(sig, subst("x_{$0}", idx));
		fLateq->addInputSigFormula(subst("x_{$0}(t)", idx));
		gDocNoticeFlagMap["inputsigs"] = true;
		return generateCacheCode(sig, subst("x_{$0}(t)", idx));
	}
}


/** Unused for the moment ! */
string DocCompiler::generateOutput (Tree sig, const string& idx, const string& arg)
{
	string dst;
	
	if (fLateq->outputs() == 1) {
		dst = subst("y(t)", idx);
		gDocNoticeFlagMap["outputsig"] = true;
	} else {
		dst = subst("y_{$0}(t)", idx);
		gDocNoticeFlagMap["outputsigs"] = true;
	}
	
	fLateq->addOutputSigFormula(subst("$0 = $1", dst, arg));
	return dst;
}


/*****************************************************************************
							   BINARY OPERATION
*****************************************************************************/

/**
 * Generate binary operations, managing priority parenthesis.
 *
 * @param	sig			The signal expression to treat.
 * @param	opcode		The operation code, as described in gBinOpLateqTable.
 * @param	arg1		The first operand.
 * @param	arg2		The second operand.
 * @param	priority	The priority of the environment of the expression.
 *
 * @return	<string>	The LaTeX code translation of the signal, cached.
 *
 * @remark	The case of LaTeX frac{}{} is special.
 *
 * @todo	Handle integer arithmetics, by testing arguments type,
 * and printing dedicated operators (\oplus, \odot, \ominus, \oslash).
 */
string DocCompiler::generateBinOp(Tree sig, int opcode, Tree arg1, Tree arg2, int priority)
{
	string s;
	int thisPriority = gBinOpLateqTable[opcode]->fPriority;
	
	/* Priority parenthesis handling. */
	string lpar = "";
	string rpar = "";
	if (thisPriority < priority) {
		lpar = " \\left(";
		rpar = "\\right) ";
	}
	
	Type t1 = getSigType(arg1);
	Type t2 = getSigType(arg2);
	bool intOpDetected = false;
	if ( (t1->nature() == kInt) && (t2->nature() == kInt) ) {
		intOpDetected = true;
	}
	
	string op;
	if(!intOpDetected) {
		op = gBinOpLateqTable[opcode]->fName;
	} else {
		switch (opcode) {
			case kAdd:
				op = "\\oplus";
				gDocNoticeFlagMap["intplus"] = true;
				break;
			case kSub:
				op = "\\ominus";
				gDocNoticeFlagMap["intminus"] = true;
				break;
			case kMul:
				op = "\\odot";
				gDocNoticeFlagMap["intmult"] = true;
				break;
			case kDiv:
				op = "\\oslash";
				gDocNoticeFlagMap["intdiv"] = true;
				break;
			default:
				op = gBinOpLateqTable[opcode]->fName;
				break;
		}
	}
	
	/* LaTeX frac{}{} handling VS general case. */
	if ( (opcode == kDiv) && (!intOpDetected) ) { 
		s = subst("$0\\frac{$1}{$2}$3", lpar, CS(arg1, 0), CS(arg2, 0), rpar);
	} else {
		s = subst("$0$1 $2 $3$4", lpar, CS(arg1, thisPriority), op, CS(arg2, thisPriority), rpar);
	}
	
	if (opcode == kMul) {
		gDocNoticeFlagMap["cdot"] = true;
	}
	
	return generateCacheCode(sig, s);
}


/*****************************************************************************
							   Primitive Operations
*****************************************************************************/

string DocCompiler::generateFFun(Tree sig, Tree ff, Tree largs, int priority)
{
    string code = ffname(ff);
    code += '(';
    string sep = "";
    for (int i = 0; i< ffarity(ff); i++) {
        code += sep;
        code += CS(nth(largs, i), priority);
        sep = ", ";
    }
    code += ')';
	
	gDocNoticeFlagMap["foreignfun"] = true;

    return "\\mathrm{ff"+code+"}";
}


/*****************************************************************************
							   CACHE CODE
*****************************************************************************/

void DocCompiler::getTypedNames(Type t, const string& prefix, string& ctype, string& vname)
{
    if (t->nature() == kInt) {
        ctype = "int"; vname = subst("$0", getFreshID(prefix));
    } else {
        ctype = ifloat(); vname = subst("$0", getFreshID(prefix));
    }
}


/**
 * Test if exp is very simple that is it
 * can't be considered a real component
 * @param exp the signal we want to test
 * @return true if it a very simple signal
 */
static bool isVerySimpleFormula(Tree sig)
{
	int		i;
	double	r;
	Tree 	type, name, file, label, c, x, y, z;
	
	return 	isSigInt(sig, &i) 
	|| 	isSigReal(sig, &r)
	||	isSigInput(sig, &i)
	||	isSigFConst(sig, type, name, file)
	||	isSigButton(sig, label)
	||	isSigCheckbox(sig, label)
	||	isSigVSlider(sig, label,c,x,y,z)
	||	isSigHSlider(sig, label,c,x,y,z)
	||	isSigNumEntry(sig, label,c,x,y,z)
	;
}


string DocCompiler::generateCacheCode(Tree sig, const string& exp)
{
	//cerr << "!! entering generateCacheCode with sig=\"" << ppsig(sig) << "\"" << endl;	
	
	string 		vname, ctype, code, vectorname;
	
	int 		sharing = getSharingCount(sig);
	Occurences* o = fOccMarkup.retrieve(sig);
	
	// check reentrance
    if (getCompiledExpression(sig, code)) {
		//cerr << "!! generateCacheCode called a true getCompiledExpression" << endl;
        return code;
    }
	
	// check for expression occuring in delays
	if (o->getMaxDelay()>0) {
        if (getVectorNameProperty(sig, vectorname)) {
			return exp;
		}
        getTypedNames(getSigType(sig), "r", ctype, vname);
		gDocNoticeFlagMap["recursigs"] = true;
		//cerr << "- r : generateCacheCode : vame=\"" << vname << "\", for sig=\"" << ppsig(sig) << "\"" << endl;
        if (sharing>1) {
			//cerr << "      generateCacheCode calls generateDelayVec(generateVariableStore) on vame=\"" << vname << "\"" << endl;            
            return generateDelayVec(sig, generateVariableStore(sig,exp), ctype, vname, o->getMaxDelay());
        } else {
			//cerr << "      generateCacheCode calls generateDelayVec(exp) on vame=\"" << vname << "\"" << endl;            
		    return generateDelayVec(sig, exp, ctype, vname, o->getMaxDelay());
        }
	} 
	else if (sharing == 1 || getVectorNameProperty(sig, vectorname) || isVerySimpleFormula(sig)) {
		//cerr << "! generateCacheCode : sharing == 1 : return \"" << exp << "\"" << endl;
        return exp;
	} 
	else if (sharing > 1) {
		//cerr << "! generateCacheCode : sharing > 1 : return \"" << exp << "\"" << endl;
        return generateVariableStore(sig, exp);
	} 
	else {
        cerr << "Error in sharing count (" << sharing << ") for " << *sig << endl;
		exit(1);
	}
	
	return "Error in generateCacheCode";
}


string DocCompiler::generateVariableStore(Tree sig, const string& exp)
{
    string      vname, ctype;
    Type        t = getSigType(sig);
	
    switch (t->variability()) {
			
        case kKonst :
            getTypedNames(t, "k", ctype, vname); ///< "k" for constants.
            fLateq->addConstSigFormula(subst("$0 = $1", vname, exp));
			gDocNoticeFlagMap["constsigs"] = true;
			return vname;
			
        case kBlock :
            getTypedNames(t, "p", ctype, vname); ///< "p" for "parameter".
            fLateq->addParamSigFormula(subst("$0(t) = $1", vname, exp));
			gDocNoticeFlagMap["paramsigs"] = true;
			setVectorNameProperty(sig, vname);
			return subst("$0(t)", vname);
			
        case kSamp :
			if(getVectorNameProperty(sig, vname)) {
				return subst("$0(t)", vname);
			} else {
				getTypedNames(t, "s", ctype, vname);
				//cerr << "- generateVariableStore : \"" << subst("$0(t) = $1", vname, exp) << "\"" << endl;
				fLateq->addStoreSigFormula(subst("$0(t) = $1", vname, exp));
				gDocNoticeFlagMap["storedsigs"] = true;
				setVectorNameProperty(sig, vname);
				return subst("$0(t)", vname);
			}
			
		default:
			assert(0);
			return "";
    }
}


/*****************************************************************************
							   	    CASTING
*****************************************************************************/


string DocCompiler::generateIntCast(Tree sig, Tree x, int priority)
{
	gDocNoticeFlagMap["intcast"] = true;
			 
	return generateCacheCode(sig, subst("\\mathrm{int}\\left($0\\right)", CS(x, 0)));
}


/**
 * @brief Don't generate float cast !
 *
 * It is just a kind of redirection.
 * Calling generateCacheCode ensures to create a new 
 * variable name if the input signal expression is shared.
 */
string DocCompiler::generateFloatCast (Tree sig, Tree x, int priority)
{
	return generateCacheCode(sig, subst("$0", CS(x, priority)));
}


/*****************************************************************************
							user interface elements
*****************************************************************************/

string DocCompiler::generateButton(Tree sig, Tree path)
{
	string vname = getFreshID("{u_b}");
	string varname = vname + "(t)";
	fLateq->addUISigFormula(getUIDir(path), prepareBinaryUI(varname, path));
	gDocNoticeFlagMap["buttonsigs"] = true;
	return generateCacheCode(sig, varname);
}

string DocCompiler::generateCheckbox(Tree sig, Tree path)
{
	string vname = getFreshID("{u_c}");
	string varname = vname + "(t)";
	fLateq->addUISigFormula(getUIDir(path), prepareBinaryUI(varname, path));
	gDocNoticeFlagMap["checkboxsigs"] = true;
	return generateCacheCode(sig, varname);
}

string DocCompiler::generateVSlider(Tree sig, Tree path, Tree cur, Tree min, Tree max, Tree step)
{
	string varname = getFreshID("{u_s}") + "(t)";
	fLateq->addUISigFormula(getUIDir(path), prepareIntervallicUI(varname, path, cur, min, max));
	gDocNoticeFlagMap["slidersigs"] = true;
	return generateCacheCode(sig, varname);
}

string DocCompiler::generateHSlider(Tree sig, Tree path, Tree cur, Tree min, Tree max, Tree step)
{
	string varname = getFreshID("{u_s}") + "(t)";
	fLateq->addUISigFormula(getUIDir(path), prepareIntervallicUI(varname, path, cur, min, max));
	gDocNoticeFlagMap["slidersigs"] = true;
	return generateCacheCode(sig, varname);
}

string DocCompiler::generateNumEntry(Tree sig, Tree path, Tree cur, Tree min, Tree max, Tree step)
{
	string varname = getFreshID("{u_n}") + "(t)";		
	fLateq->addUISigFormula(getUIDir(path), prepareIntervallicUI(varname, path, cur, min, max));
	gDocNoticeFlagMap["nentrysigs"] = true;
	return generateCacheCode(sig, varname);
}


string DocCompiler::generateVBargraph(Tree sig, Tree path, Tree min, Tree max, const string& exp)
{
	string varname = getFreshID("{u_g}");

	Type t = getSigType(sig);
	switch (t->variability()) {

		case kKonst :
			break;

		case kBlock :
			break;

		case kSamp :
			break;
	}
    return generateCacheCode(sig, varname);
}


string DocCompiler::generateHBargraph(Tree sig, Tree path, Tree min, Tree max, const string& exp)
{
	string varname = getFreshID("{u_g}");

	Type t = getSigType(sig);
	switch (t->variability()) {

		case kKonst :
			break;

		case kBlock :
			break;

		case kSamp :
			break;
	}
    return generateCacheCode(sig, varname);
}


string DocCompiler::generateAttach (Tree sig, Tree x, Tree y, int priority)
{
	string vname;
	string exp;
	
	CS(y, priority);
	exp = CS(x, priority);
	
	if(getVectorNameProperty(x, vname)) {
		setVectorNameProperty(sig, vname);
	}

	return generateCacheCode(sig, exp);
}




/*****************************************************************************
							   	    TABLES
*****************************************************************************/



/*----------------------------------------------------------------------------
						sigGen : initial table content
----------------------------------------------------------------------------*/

string DocCompiler::generateSigGen(Tree sig, Tree content)
{
	string ltqname = getFreshID("table");
	
	//cerr << "!!! generateSigGen : " << endl;
	//cerr << "  * sig = " << ppsig(sig) << endl;
	//cerr << "  * content = " << ppsig(content) << endl;

	return "\\mathrm{"+ltqname+"}";
}

string DocCompiler::generateStaticSigGen(Tree sig, Tree content)
{
	string ltqname = getFreshID("table");

	//return CS(content, 0);
	return "\\mathrm{"+ltqname+"}";
}


/*----------------------------------------------------------------------------
						sigTable : table declaration
----------------------------------------------------------------------------*/

string DocCompiler::generateTable(Tree sig, Tree tsize, Tree content, int priority)
{
	string 		generator(CS(content, priority));
	string		ctype, vname;
	int 		size;

	if (!isSigInt(tsize, &size)) {
		//fprintf(stderr, "error in DocCompiler::generateTable()\n"); exit(1);
		cerr << "error in DocCompiler::generateTable() : "
			 << *tsize
			 << " is not an integer expression "
			 << endl;
	}
	// definition du nom et du type de la table
	// A REVOIR !!!!!!!!!
	Type t = getSigType(content);//, tEnv);
	if (t->nature() == kInt) {
		vname = getFreshID("itbl");
		ctype = "int";
	} else {
		vname = getFreshID("ftbl");
		ctype = ifloat();
	}

	// on retourne le nom de la table
	return vname;
}


string DocCompiler::generateStaticTable(Tree sig, Tree tsize, Tree content)
{
	//cerr << "??? generateStaticTable : " << endl;
	//cerr << "  * sig = " << ppsig(sig) << endl;
	//cerr << "  * content = " << ppsig(content) << endl;
	
	string 		exp = CS(content,0);
	string		vname, ctype;
	Type        t = getSigType(content);

	if (!getVectorNameProperty(content, vname)) {
		getTypedNames(t, "w", ctype, vname);
		gDocNoticeFlagMap["tablesigs"] = true;
		fLateq->addStoreSigFormula(subst("$0(t) = $1", vname, exp));
		setVectorNameProperty(content, vname);
		setVectorNameProperty(sig, vname);
	}
	return vname;
}


/*----------------------------------------------------------------------------
						sigWRTable : table assignement
----------------------------------------------------------------------------*/

string DocCompiler::generateWRTbl(Tree sig, Tree tbl, Tree idx, Tree data, int priority)
{
	string tblName(CS(tbl, priority));
	fLateq->addTableSigFormula(subst("$0[$1] = $2", tblName, CS(idx, priority), CS(data, priority)));
	return tblName;
}


/*----------------------------------------------------------------------------
						sigRDTable : table access
----------------------------------------------------------------------------*/

string DocCompiler::generateRDTbl(Tree sig, Tree tbl, Tree idx, int priority)
{
	// YO le 21/04/05 : La lecture des tables n'était pas mise dans le cache
	// et donc le code était dupliqué (dans tester.dsp par exemple)
	//return subst("$0[$1]", CS(tEnv, tbl), CS(tEnv, idx));

	//cerr << "generateRDTable " << *sig << endl;
	// test the special case of a read only table that can be compiled
	// has a static member
	Tree 	id, size, content;
	if(	isSigTable(tbl, id, size, content) ) {
		string tblname;
		if (!getCompiledExpression(tbl, tblname)) {
			tblname = setCompiledExpression(tbl, generateStaticTable(tbl, size, content));
		}
		return generateCacheCode(sig, subst("$0($1)", tblname, CS(idx, priority)));
	} else {
		return generateCacheCode(sig, subst("$0($1)", CS(tbl, priority), CS(idx, priority)));
	}
}



/*****************************************************************************
							   RECURSIONS
*****************************************************************************/


/**
 * Generate code for a projection of a group of mutually recursive definitions
 */
string DocCompiler::generateRecProj(Tree sig, Tree r, int i, int priority)
{
    string  vname;
    Tree    var, le;
	
	//cerr << "*** generateRecProj sig : \"" << ppsig(sig) << "\"" << endl;            

    if ( ! getVectorNameProperty(sig, vname)) {
        assert(isRec(r, var, le));
		//cerr << "    generateRecProj has NOT YET a vname : " << endl;            
		//cerr << "--> generateRecProj calls generateRec on \"" << ppsig(sig) << "\"" << endl;            
        generateRec(r, var, le, priority);
        assert(getVectorNameProperty(sig, vname));
		//cerr << "<-- generateRecProj vname : \"" << subst("$0(t)", vname) << "\"" << endl;            
    } else {
		//cerr << "(generateRecProj has already a vname : \"" << subst("$0(t)", vname) << "\")" << endl;            
	}
    return subst("$0(t)", vname);
}


/**
 * Generate code for a group of mutually recursive definitions
 */
void DocCompiler::generateRec(Tree sig, Tree var, Tree le, int priority)
{
    int             N = len(le);

    vector<bool>    used(N);
    vector<int>     delay(N);
    vector<string>  vname(N);
    vector<string>  ctype(N);

    // prepare each element of a recursive definition
    for (int i=0; i<N; i++) {
        Tree    e = sigProj(i,sig);     // recreate each recursive definition
        if (fOccMarkup.retrieve(e)) {
            // this projection is used
            used[i] = true;
			//cerr << "generateRec : used[" << i << "] = true" << endl;            
            getTypedNames(getSigType(e), "r", ctype[i],  vname[i]);
			gDocNoticeFlagMap["recursigs"] = true;
			//cerr << "- r : generateRec setVectorNameProperty : \"" << vname[i] << "\"" << endl;
			setVectorNameProperty(e, vname[i]);
            delay[i] = fOccMarkup.retrieve(e)->getMaxDelay();
        } else {
            // this projection is not used therefore
            // we should not generate code for it
            used[i] = false;
			//cerr << "generateRec : used[" << i << "] = false" << endl;
        }
    }

    // generate delayline for each element of a recursive definition
    for (int i=0; i<N; i++) {
        if (used[i]) {
            generateDelayLine(ctype[i], vname[i], delay[i], CS(nth(le,i), priority));
        }
    }
}


/*****************************************************************************
							   PREFIX, DELAY A PREFIX VALUE
*****************************************************************************/

/**
 * Generate LaTeX code for "prefix", a 1­sample-delay explicitely initialized.
 *
 * @param	sig			The signal expression to treat.
 * @param	x			The initial value for the delay line.
 * @param	e			The value for the delay line, after initialization.
 * @param	priority	The priority of the environment of the expression.
 *
 * @return	<string>	The LaTeX code translation of the signal, cached.
 */
string DocCompiler::generatePrefix (Tree sig, Tree x, Tree e, int priority)
{
	string var  = getFreshID("m");
	string exp0 = CS(x, priority);
	string exp1 = CS(e, priority); // ensure exp1 is compiled to have a vector name
	string vecname;

	if (! getVectorNameProperty(e, vecname)) {
		cerr << "No vector name for : " << ppsig(e) << endl;
		assert(0);
	}
	
	string ltqPrefixDef;
	ltqPrefixDef += subst("$0(t) = \n", var);
	ltqPrefixDef += "\\left\\{\\begin{array}{ll}\n";
	ltqPrefixDef += subst("$0 & \\mbox{, when \\,} t = 0\\\\\n", exp0);
	ltqPrefixDef += subst("$0 & \\mbox{, when \\,} t > 0\n", subst("$0(t\\!-\\!1)", vecname));
	ltqPrefixDef += "\\end{array}\\right.";
	
	fLateq->addPrefixSigFormula(ltqPrefixDef);
	gDocNoticeFlagMap["prefixsigs"] = true;
	
	return generateCacheCode(sig, subst("$0(t)", var));
}


/*****************************************************************************
							   IOTA(n)
*****************************************************************************/

/**
 * Generate a "iota" time function, n-cyclical.
 */
string DocCompiler::generateIota (Tree sig, Tree n)
{
	int size;
	if (!isSigInt(n, &size)) { fprintf(stderr, "error in generateIota\n"); exit(1); }
	return subst(" t \\bmod{$0} ", docT(size));
}



// a revoir en utilisant la lecture de table et en partageant la construction de la paire de valeurs


/**
 * Generate a select2 code
 */
string DocCompiler::generateSelect2  (Tree sig, Tree sel, Tree s1, Tree s2, int priority)
{
    string var  = getFreshID("q");
	string expsel = CS(sel, priority);
	string exps1 = CS(s1, priority);
	string exps2 = CS(s2, priority);
	
	string ltqSelDef;
	ltqSelDef += subst("$0(t) = \n", var);
	ltqSelDef += "\\left\\{\\begin{array}{ll}\n";
	ltqSelDef += subst("$0 & \\mbox{if \\,} $1 = 0\\\\\n", generateVariableStore(s1, exps1), expsel);
	ltqSelDef += subst("$0 & \\mbox{if \\,} $1 = 1\n", generateVariableStore(s2, exps2), expsel);
	ltqSelDef += "\\end{array}\\right.";
	
	fLateq->addSelectSigFormula(ltqSelDef);
	gDocNoticeFlagMap["selectionsigs"] = true;
	
	return generateCacheCode(sig, subst("$0(t)", var));
}


/**
 * Generate a select3 code
 */
string DocCompiler::generateSelect3  (Tree sig, Tree sel, Tree s1, Tree s2, Tree s3, int priority)
{
	string var  = getFreshID("q");
	string expsel = CS(sel, priority);
	string exps1 = CS(s1, priority);
	string exps2 = CS(s2, priority);
	string exps3 = CS(s3, priority);
	
	string ltqSelDef;
	ltqSelDef += subst("$0(t) = \n", var);
	ltqSelDef += "\\left\\{\\begin{array}{ll}\n";
	ltqSelDef += subst("$0 & \\mbox{if \\,} $1 = 0\\\\\n", generateVariableStore(s1, exps1), expsel);
	ltqSelDef += subst("$0 & \\mbox{if \\,} $1 = 1\\\\\n", generateVariableStore(s2, exps2), expsel);
	ltqSelDef += subst("$0 & \\mbox{if \\,} $1 = 2\n", generateVariableStore(s3, exps3), expsel);
	ltqSelDef += "\\end{array}\\right.";
	
	fLateq->addSelectSigFormula(ltqSelDef);
	gDocNoticeFlagMap["selectionsigs"] = true;
	
	return generateCacheCode(sig, subst("$0(t)", var));
}


/**
 * retrieve the type annotation of sig
 * @param sig the signal we want to know the type
 */
string DocCompiler::generateXtended 	(Tree sig, int priority)
{
	xtended* 		p = (xtended*) getUserData(sig);
	vector<string> 	args;
	vector<Type> 	types;

	for (int i=0; i<sig->arity(); i++) {
		args.push_back(CS(sig->branch(i), 0));
		types.push_back(getSigType(sig->branch(i)));
	}

	if (p->needCache()) {
		//cerr << "!! generateXtended : <needCache> : calls generateCacheCode(sig, p->generateLateq(fLateq, args, types))" << endl;
		return generateCacheCode(sig, p->generateLateq(fLateq, args, types));
	} else {
		//cerr << "!! generateXtended : <do not needCache> : calls p->generateLateq(fLateq, args, types)" << endl;
		return p->generateLateq(fLateq, args, types);
	}
}



//------------------------------------------------------------------------------------------------


/*****************************************************************************
						vector name property
*****************************************************************************/

/**
 * Set the vector name property of a signal, the name of the vector used to
 * store the previous values of the signal to implement a delay.
 * @param sig the signal expression.
 * @param vecname the string representing the vector name.
 * @return true is already compiled
 */
void DocCompiler::setVectorNameProperty(Tree sig, const string& vecname)
{
	fVectorProperty.set(sig, vecname);
}


/**
 * Get the vector name property of a signal, the name of the vector used to
 * store the previous values of the signal to implement a delay.
 * @param sig the signal expression.
 * @param vecname the string where to store the vector name.
 * @return true if the signal has this property, false otherwise
 */

bool DocCompiler::getVectorNameProperty(Tree sig, string& vecname)
{
    return fVectorProperty.get(sig, vecname);
}



/*****************************************************************************
							   N-SAMPLE FIXED DELAY : sig = exp@delay

	case 1-sample max delay :
		Y(t-0)	Y(t-1)
		Temp	Var						gLessTempSwitch = false
		V[0]	V[1]					gLessTempSwitch = true

	case max delay < gMaxCopyDelay :
		Y(t-0)	Y(t-1)	Y(t-2)  ...
		Temp	V[0]	V[1]	...		gLessTempSwitch = false
		V[0]	V[1]	V[2]	...		gLessTempSwitch = true

	case max delay >= gMaxCopyDelay :
		Y(t-0)	Y(t-1)	Y(t-2)  ...
		Temp	V[0]	V[1]	...
		V[0]	V[1]	V[2]	...


*****************************************************************************/

/**
 * Generate code for accessing a delayed signal. The generated code depend of
 * the maximum delay attached to exp and the gLessTempSwitch.
 *
 * @todo Priorités à revoir pour le parenthésage (associativité de - et /),
 * avec gBinOpLateqTable dans binop.cpp.
 */
string DocCompiler::generateFixDelay (Tree sig, Tree exp, Tree delay, int priority)
{
	int d;
	string vecname;
	
	CS(exp, 0); // ensure exp is compiled to have a vector name
	
	if (! getVectorNameProperty(exp, vecname)) {
		cerr << "No vector name for : " << ppsig(exp) << endl;
		assert(0);
	}
	
	if (isSigInt(delay, &d) && (d == 0)) {
		//cerr << "@ generateFixDelay : d = " << d << endl;
		return subst("$0(t)", vecname);
	} else {
		//cerr << "@ generateFixDelay : d = " << d << endl;
		return subst("$0(t\\!-\\!$1)", vecname, CS(delay, 7));
	}
}


/**
 * Generate code for the delay mecchanism. The generated code depend of the
 * maximum delay attached to exp and the "less temporaries" switch
 */
string DocCompiler::generateDelayVec(Tree sig, const string& exp, const string& ctype, const string& vname, int mxd)
{
	string s = generateDelayVecNoTemp(sig, exp, ctype, vname, mxd);
	if (getSigType(sig)->variability() < kSamp) {
        return exp;
	} else {
		return s;
	}
}


/**
 * Generate code for the delay mecchanism without using temporary variables
 */
string DocCompiler::generateDelayVecNoTemp(Tree sig, const string& exp, const string& ctype, const string& vname, int mxd)
{
    assert(mxd > 0);

	//cerr << "  entering generateDelayVecNoTemp" << endl;
	
	string vectorname;

	// if generateVariableStore has already tagged sig, no definition is needed.
	if(getVectorNameProperty(sig, vectorname)) { 
		return subst("$0(t)", vectorname);
	} else {
        fLateq->addRecurSigFormula(subst("$0(t) = $1", vname, exp));
        setVectorNameProperty(sig, vname);
        return subst("$0(t)", vname);
	}
}


/**
 * Generate code for the delay mecchanism without using temporary variables
 */
void DocCompiler::generateDelayLine(const string& ctype, const string& vname, int mxd, const string& exp)
{
    //assert(mxd > 0);
    if (mxd == 0) {
        fLateq->addRecurSigFormula(subst("$0(t) = $1", vname, exp));
    } else {
        fLateq->addRecurSigFormula(subst("$0(t) = $1", vname, exp));
	}
}




/****************************************************************
			User interface element utilities.
 *****************************************************************/


/**
 * @brief Get the directory of a user interface element.
 *
 * Convert the input reversed path tree into a string.
 * The name of the UI is stripped (the head of the path tree), 
 * the rest of the tree is a list of pointed pairs, where the names 
 * are contained by the tail of these pointed pairs.
 * Metadatas (begining by '[') are stripped.
 * 
 * @param[in]	pathname	The path tree to convert.
 * @return		<string>	A directory-like string. 
 */
string DocCompiler::getUIDir(Tree pathname)
{	
	//cerr << "Documentator : getUIDir : print(pathname, stdout) = "; print(pathname, stdout); cerr << endl;
	string s;
	Tree dir = reverse(tl(pathname));
	while (!isNil(dir)) { 
		string tmp = tree2str(tl(hd(dir)));
		if ( (tmp[0] != '[') && (!tmp.empty()) ) {
			s += tmp + '/';
		}
		dir = tl(dir);
	}
	return s;
}


/**
 * @brief Prepare binary user interface elements (button, checkbox).
 *
 * - Format a LaTeX output string as a supertabular row with 5 columns :
 * "\begin{supertabular}{rllll}". @see Lateq::printHierarchy
 * - The UI range is only a set of two values : {0, 1}.
 * - The UI current value is automatically 0.
 * 
 * @param[in]	name		The LaTeX name of the UI signal (eg. "{u_b}_{i}(t)").
 * @param[in]	path		The path tree to parse.
 * @return		<string>	The LaTeX output string. 
 */
string DocCompiler::prepareBinaryUI(const string& name, Tree path)
{	
	string label, unit;
	getUIDocInfos(path, label, unit);
	string s = "";
	label = (label.size()>0) ? ("\\textsf{\""+label+"\"} :") : "";
	unit = (unit.size()>0) ? ("\\,\\mathrm{"+unit+"}") : "";
	s += label;
	s += " & $" + name + "$";
	s += " & $\\in$ & $\\left\\{\\,0" + unit + ", 1" + unit +"\\,\\right\\}$";
	s += " & $(\\mbox{" + gDocMathStringMap["defaultvalue"] + "} = 0" + unit + ")$\\\\";
	return s;
}


/**
 * @brief Prepare "intervallic" user interface elements (sliders, nentry).
 *
 * - Format a LaTeX output string as a supertabular row with 5 columns :
 * "\begin{supertabular}{rllll}". @see Lateq::printHierarchy
 * - The UI range is an bounded interval : [tmin, tmax].
 * - The UI current value is tcur.
 * 
 * @param[in]	name		The LaTeX name of the UI signal (eg. "{u_s}_{i}(t)").
 * @param[in]	path		The path tree to parse.
 * @param[in]	tcur		The current UI value tree to convert.
 * @param[in]	tmin		The minimum UI value tree to convert.
 * @param[in]	tmax		The maximum UI value tree to convert.
 * @return		<string>	The LaTeX output string. 
 */
string DocCompiler::prepareIntervallicUI(const string& name, Tree path, Tree tcur, Tree tmin, Tree tmax)
{	
	string label, unit, cur, min, max;
	getUIDocInfos(path, label, unit);
	cur = docT(tree2float(tcur));
	min = docT(tree2float(tmin));
	max = docT(tree2float(tmax));
	
	string s = "";
	label = (label.size()>0) ? ("\\textsf{\""+label+"\"} :") : "";
	unit = (unit.size()>0) ? ("\\,\\mathrm{"+unit+"}") : "";
	s += label;
	s += " & $" + name + "$";
	s += " & $\\in$ & $\\left[\\," + min + unit + ", " + max + unit +"\\,\\right]$";
	s += " & $(\\mbox{" + gDocMathStringMap["defaultvalue"] + "} = " + cur + unit + ")$\\\\";
	return s;
}


/** 
 * Get information on a user interface element for documentation.
 *
 * @param[in]	path	The UI full pathname to parse.
 * @param[out]	label	The place to store the UI name. 
 * @param[out]	unit	The place to store the UI unit.
 */
void DocCompiler::getUIDocInfos(Tree path, string& label, string& unit)
{
	label = "";
	unit = "";
	
    map<string, set<string> >   metadata;
    extractMetadata(tree2str(hd(path)), label, metadata);
	
	set<string> myunits = metadata["unit"];
//	for (set<string>::iterator i = myunits.begin(); i != myunits.end(); i++) {
//		cerr << "Documentator : getUIDocInfos : metadata[\"unit\"] = " << *i << endl;
//	}
	for (map<string, set<string> >::iterator i = metadata.begin(); i != metadata.end(); i++) {
		const string& key = i->first;
		const set<string>& values = i->second;
		for (set<string>::iterator j = values.begin(); j != values.end(); j++) {
			if(key == "unit") unit += *j;
		}
	}
}


/**
 * Extracts metadata from a UI label : 'vol [unit: dB]' -> 'vol' + metadata map.
 */
static void extractMetadata(const string& fulllabel, string& label, map<string, set<string> >& metadata)
{
    enum {kLabel, kEscape1, kEscape2, kEscape3, kKey, kValue};
    int state = kLabel; int deep = 0;
    string key, value;
	
    for (unsigned int i=0; i < fulllabel.size(); i++) {
        char c = fulllabel[i];
        switch (state) {
            case kLabel :
                assert (deep == 0);
                switch (c) {
                    case '\\' : state = kEscape1; break;
                    case '[' : state = kKey; deep++; break;
                    default : label += c;
                }
                break;
				
            case kEscape1 :
                label += c;
                state = kLabel;
                break;
				
            case kEscape2 :
                key += c;
                state = kKey;
                break;
				
            case kEscape3 :
                value += c;
                state = kValue;
                break;
				
            case kKey :
                assert (deep > 0);
                switch (c) {
                    case '\\' :  state = kEscape2;
						break;
						
                    case '[' :  deep++;
						key += c;
						break;
						
                    case ':' :  if (deep == 1) {
						state = kValue;
					} else {
						key += c;
					}
						break;
                    case ']' :  deep--;
						if (deep < 1) {
							metadata[rmWhiteSpaces(key)].insert("");
							state = kLabel;
							key="";
							value="";
						} else {
							key += c;
						}
						break;
                    default :   key += c;
                }
                break;
				
            case kValue :
                assert (deep > 0);
                switch (c) {
                    case '\\' : state = kEscape3;
						break;
						
                    case '[' :  deep++;
						value += c;
						break;
						
                    case ']' :  deep--;
						if (deep < 1) {
							metadata[rmWhiteSpaces(key)].insert(rmWhiteSpaces(value));
							state = kLabel;
							key="";
							value="";
						} else {
							value += c;
						}
						break;
                    default :   value += c;
                }
                break;
				
            default :
                cerr << "ERROR unrecognized state (in extractMetadata) : " << state << endl;
        }
    }
    label = rmWhiteSpaces(label);
}


/**
 * rmWhiteSpaces(): Remove the leading and trailing white spaces of a string
 * (but not those in the middle of the string)
 */
static string rmWhiteSpaces(const string& s)
{
    size_t i = s.find_first_not_of(" \t");
    size_t j = s.find_last_not_of(" \t");
	
    if ( (i != string::npos) & (j != string::npos) ) {
        return s.substr(i, 1+j-i);
    } else {
        return "";
    }
}

