#ifndef __FAUST_GLOBAL__
#define __FAUST_GLOBAL__


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <set>
#include <map>

/*
#include "compatibility.hh"
#include "signals.hh"
#include "sigtype.hh"
#include "sigtyperules.hh"
#include "sigprint.hh"
#include "simplify.hh"
#include "privatise.hh"
#include "recursivness.hh"

#include "propagate.hh"
#include "errormsg.hh"
#include "ppbox.hh"
#include "enrobage.hh"
#include "eval.hh"
#include "description.hh"
#include "floats.hh"
#include "doc.hh"
*/

/*
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
*/

#ifndef WIN32
#include <unistd.h>
#endif

//#include "llvm_code_container.hh"

#include <llvm/Module.h>

#include "sourcereader.hh"
#include "tree.hh"

struct global {

    Tree 			gResult;
    Tree 			gResult2;

    SourceReader	gReader;

    map<Tree, set<Tree> > gMetaDataSet;
    string gDocLang;

    //-- globals
    string          gFaustSuperSuperDirectory;
    string          gFaustSuperDirectory;
    string          gFaustDirectory;
    string          gMasterDocument;
    string          gMasterDirectory;
    string          gMasterName;
    string          gDocName;
    Tree			gExpandedDefList;

    //-- command line arguments
    bool            gDetailsSwitch;
    bool            gDrawSignals;
    bool            gShadowBlur;        // note: svg2pdf doesn't like the blur filter
     bool			gStripDocSwitch;	// Strip <mdoc> content from doc listings.
    int            	gFoldThreshold;
    int            	gMaxNameSize;
    bool			gSimpleNames;
    bool            gSimplifyDiagrams;
    bool			gLessTempSwitch;
    int				gMaxCopyDelay;
    string			gOutputFile;
  
    bool            gVectorSwitch;
    bool            gDeepFirstSwitch;
    int             gVecSize;
    int             gVectorLoopVariant;
    int             gVecLoopSize;

    bool            gOpenMPSwitch;
    bool            gOpenMPLoop;
    bool            gSchedulerSwitch;
    bool            gOpenCLSwitch;
    bool            gCUDASwitch;
    bool			gGroupTaskSwitch;
    bool			gFunTaskSwitch;

    bool            gUIMacroSwitch;
    bool            gDumpNorm;

    int             gTimeout;            // time out to abort compiler (in seconds)

    int             gFloatSize;

    bool			gPrintFileListSwitch;
    bool			gInlineArchSwitch;

    bool			gDSPStruct;

    string			gClassName;

    llvm::Module*   gModule;
    const char*     gInputString;
    
    bool			gLstDependenciesSwitch;     ///< mdoc listing management.
    bool			gLstMdocTagsSwitch;         ///< mdoc listing management.
    bool			gLstDistributedSwitch;      ///< mdoc listing management.
    
    map<string, string>		gDocMetadatasStringMap;
    set<string>				gDocMetadatasKeySet;
    
    map<string, string>		gDocAutodocStringMap;
    set<string>				gDocAutodocKeySet;
    
    map<string, bool>       gDocNoticeFlagMap;
    
    map<string, string>		gDocMathStringMap;
    
    vector<Tree>            gDocVector;				///< Contains <mdoc> parsed trees: DOCTXT, DOCEQN, DOCDGM.
    
    map<string, string>     gDocNoticeStringMap;
    set<string>             gDocNoticeKeySet;
    
    set<string>				gDocMathKeySet;
    
    bool                    gLatexDocSwitch;		// Only LaTeX outformat is handled for the moment.

    Tabber TABBER;
  
    global():TABBER(1)
    {
        gDetailsSwitch  = false;
        gDrawSignals    = false;
        gShadowBlur     = false;	// note: svg2pdf doesn't like the blur filter
        gStripDocSwitch = false;	// Strip <mdoc> content from doc listings.
        gFoldThreshold 	= 25;
        gMaxNameSize 	= 40;
        gSimpleNames 	= false;
        gSimplifyDiagrams = false;
        gLessTempSwitch = false;
        gMaxCopyDelay	= 16;
   
        gVectorSwitch   = false;
        gDeepFirstSwitch = false;
        gVecSize        = 32;
        gVectorLoopVariant = 0;
        gVecLoopSize = 0;

        gOpenMPSwitch   = false;
        gOpenMPLoop     = false;
        gSchedulerSwitch  = false;
        gOpenCLSwitch  = false;
        gCUDASwitch = false;
        gGroupTaskSwitch = false;
        gFunTaskSwitch = false;

        gUIMacroSwitch  = false;
        gDumpNorm       = false;

        gTimeout        = 120;            // time out to abort compiler (in seconds)

        gFloatSize = 1;

        gPrintFileListSwitch = false;
        gInlineArchSwitch = true;

        gDSPStruct = false;

        gClassName = "mydsp";

        gModule = 0;
        gInputString = 0;
        
        gLstDependenciesSwitch	= true; ///< mdoc listing management.
        gLstMdocTagsSwitch		= true; ///< mdoc listing management.
        gLstDistributedSwitch	= true; ///< mdoc listing management.
        
        gLatexDocSwitch = true;		// Only LaTeX outformat is handled for the moment.
    }

};

extern global* gGlobal;

#endif