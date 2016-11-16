#ifndef ctags_cxx_parser_internal_h_
#define ctags_cxx_parser_internal_h_
/*
*   Copyright (c) 2016, Szymon Tomasz Stefanek
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for parsing and scanning C++ source files
*/

#include "general.h"

#include "parse.h"

#include "cxx_tag.h"
#include "cxx_keyword.h"
#include "cxx_token.h"
#include "cxx_token_chain.h"

//
// CXX parser internal declarations.
// This file is included only by cxx_parser_*.c
//

// cxx_parser_tokenizer.c
bool cxxParserParseNextToken(void);

// cxx_parser_lambda.c
CXXToken * cxxParserOpeningBracketIsLambda(void);
bool cxxParserHandleLambda(CXXToken * pParenthesis);

// cxx_parser_block.c
bool cxxParserParseBlock(bool bExpectClosingBracket);
bool cxxParserParseBlockHandleOpeningBracket(void);

enum CXXExtractVariableDeclarationsFlags
{
	// We are parsing K&R style parameter declarations.
	CXXExtractVariableDeclarationsKnRStyleParameters = 1
};

// cxx_parser_variable.c
bool cxxParserExtractVariableDeclarations(
		CXXTokenChain * pChain,
		unsigned int uFlags
	);

// cxx_parser_function.c

bool cxxParserTokenChainLooksLikeFunctionCallParameterSet(
		CXXTokenChain * pChain
	);
bool cxxParserTokenChainLooksLikeConstructorParameterSet(
		CXXTokenChain * pChain
	);

typedef enum _CXXFunctionSignatureInfoFlag
{
	// Followed by = 0
	CXXFunctionSignatureInfoPure = 1,
	// Followed by = default
	CXXFunctionSignatureInfoDefault = (1 << 1),
	// Followed by "override"
	CXXFunctionSignatureInfoOverride = (1 << 2),
	// Followed by "final"
	CXXFunctionSignatureInfoFinal = (1 << 3),
	// Followed by = delete
	CXXFunctionSignatureInfoDelete = (1 << 4),
	// Followed by volatile
	CXXFunctionSignatureInfoVolatile = (1 << 5),
	// Is template specialization  a<x>()
	CXXFunctionSignatureInfoTemplateSpecialization = (1 << 6),
	// Is scope template specialization a<x>::b()
	// (implies that this is a template specialization too)
	CXXFunctionSignatureInfoScopeTemplateSpecialization = (1 << 7),
} CXXFunctionSignatureInfoFlag;

//
// Description of a function signature.
//
typedef struct _CXXFunctionSignatureInfo
{
	// The parenthesis token.
	// It is always contained in the chain pointed by pParenthesisContainerChain
	CXXToken * pParenthesis;
	
	// The token chain that contains the parenthesis above. May or may not
	// be the toplevel chain.
	CXXTokenChain * pParenthesisContainerChain;

	// The identifier. It's either a single token (so both pIdentifierStart
	// and pIdentifierEnd point to the same token) or multiple tokens starting
	// with the "operator" keyword. Spacing of the tokens is adjusted.
	// The identifier is always contained in the chain pointed by pIdentifierChain.
	CXXToken * pIdentifierStart;
	CXXToken * pIdentifierEnd;
	
	// The chain that pIdentifierStart, pIdentifierEnd and pScopeStart
	// belong to. It MAY be a nested chain and it may even be included in the
	// range specified by pTypeStart / pTypeEnd below!
	CXXTokenChain * pIdentifierChain;

	// Non-NULL if the signature is followed by the "const" keyword
	CXXToken * pSignatureConst;

	// Non-NULL if there is a scope before the identifier.
	// The scope ends at pIdentifierStart.
	// The scope start is always in the chain pointed by pIdentifierChain.
	CXXToken * pScopeStart;

	// Non-NULL if a return type has been identified.
	CXXToken * pTypeStart;
	CXXToken * pTypeEnd;
	// There are cases in that the type range defined above may
	// contain the identifier, scope and signature ranges.
	// This happens, for example, with functions returning
	// nasty things, like:
	//     int (*foo(void))[2]
	// It is granted that the scope and identifier are either
	// completly included or completly excluded from the type range.
	bool bTypeContainsIdentifierScopeAndSignature;
	
	// Non-NULL if there is a trailing comma after the function.
	// This is used for the special case of multiple prototypes in a single
	// declaration:
	//     RetType functionA(...), functionB(...);
	CXXToken * pTrailingComma;

	// Additional informations
	unsigned int uFlags;

} CXXFunctionSignatureInfo;

int cxxParserMaybeExtractKnRStyleFunctionDefinition(int * piCorkQueueIndex);
int cxxParserExtractFunctionSignatureBeforeOpeningBracket(int * piCorkQueueIndex);

#define CXX_MAX_EXTRACTED_PARAMETERS 24

typedef struct _CXXFunctionParameterInfo
{
	// The number of parameters found
	unsigned int uParameterCount;

	// All the tokens are references to the source chain (do not delete)
	CXXTokenChain * pChain;
	// The initial tokens of the declaration
	CXXToken * aDeclarationStarts[CXX_MAX_EXTRACTED_PARAMETERS];
	// The final tokens of the declaration
	CXXToken * aDeclarationEnds[CXX_MAX_EXTRACTED_PARAMETERS];
	// The identifier tokens (betweeh initial and final)
	CXXToken * aIdentifiers[CXX_MAX_EXTRACTED_PARAMETERS];
} CXXFunctionParameterInfo;

bool cxxParserTokenChainLooksLikeFunctionParameterList(
		CXXTokenChain * tc,
		CXXFunctionParameterInfo * pParamInfo
	);
bool cxxParserLookForFunctionSignature(
		CXXTokenChain * pChain,
		CXXFunctionSignatureInfo * pInfo,
		CXXFunctionParameterInfo * pParamInfo
	);

enum CXXEmitFunctionTagsOptions
{
	// Push the scopes defined by the function
	CXXEmitFunctionTagsPushScopes = 1
};

int cxxParserEmitFunctionTags(
		CXXFunctionSignatureInfo * pInfo,
		unsigned int uTagKind,
		unsigned int uOptions,
		int * piCorkQueueIndex
	);

void cxxParserEmitFunctionParameterTags(CXXFunctionParameterInfo * pInfo);

// cxx_parser_typedef.c
bool cxxParserParseGenericTypedef(void);
void cxxParserExtractTypedef(
		CXXTokenChain * pChain,
		bool bExpectTerminatorAtEnd
	);

// cxx_parser_namespace.c
bool cxxParserParseNamespace(void);

// cxx_parser.c
void cxxParserNewStatement(void);
bool cxxParserSkipToSemicolonOrEOF(void);
bool cxxParserParseToEndOfQualifedName(void);
bool cxxParserParseEnum(void);
bool cxxParserParseClassStructOrUnion(
		enum CXXKeyword eKeyword,
		unsigned int uTagKind,
		unsigned int uScopeType
	);
bool cxxParserParseAndCondenseCurrentSubchain(
		unsigned int uInitialSubchainMarkerTypes,
		bool bAcceptEOF
	);
bool cxxParserParseUpToOneOf(unsigned int uTokenTypes);
bool cxxParserParseIfForWhileSwitch(void);
bool cxxParserParseTemplatePrefix(void);
bool cxxParserParseUsingClause(void);
bool cxxParserParseAccessSpecifier(void);
void cxxParserAnalyzeOtherStatement(void);
bool cxxParserParseAndCondenseSubchainsUpToOneOf(
		unsigned int uTokenTypes,
		unsigned int uInitialSubchainMarkerTypes
	);
void cxxParserMarkEndLineForTagInCorkQueue(int iCorkQueueIndex);

typedef enum _CXXParserKeywordState
{
	// We are parsing a statement that comes right after
	// a typedef keyword (so we're parsing the type being typedef'd).
	CXXParserKeywordStateSeenTypedef = 1,
	// We are parsing a statement that comes right after
	// an inline keyword
	CXXParserKeywordStateSeenInline = (1 << 1),
	// We are parsing a statement that comes right after
	// a extern keyword
	CXXParserKeywordStateSeenExtern = (1 << 2),
	// We are parsing a statement that comes right after
	// a static keyword
	CXXParserKeywordStateSeenStatic = (1 << 3),
	// an "explicit" keyword has been seen
	CXXParserKeywordStateSeenExplicit = (1 << 4),
	// an "operator" keyword has been seen
	CXXParserKeywordStateSeenOperator = (1 << 5),
	// "virtual" has been seen
	CXXParserKeywordStateSeenVirtual = (1 << 6),
	// "return" has been seen
	CXXParserKeywordStateSeenReturn = (1 << 7),
	// "mutable" has been seen
	CXXParserKeywordStateSeenMutable = (1 << 8),
	// "const" has been seen at block level
	CXXParserKeywordStateSeenConst = (1 << 9),
	// "volatile" has been seen at block level
	CXXParserKeywordStateSeenVolatile = (1 << 10),
	// __attribute__((deprecated)) has been seen
	CXXParserKeywordStateSeenAttributeDeprecated = (1 << 11),
} CXXParserKeywordState;

typedef struct _CXXParserState
{
	// The current language
	langType eLanguage;

	// The identifier of the CPP language, as indicated by ctags core
	langType eCPPLanguage;
	// The identifier of the C language, as indicated by ctags core
	langType eCLanguage;
	
	// The kind options associated to the current language
	kindOption * pKindOptions;
	// The number of kind options, used mainly for checking/debug purposes
	unsigned int uKindOptionCount;
	
	// The fields associated to the current language
	fieldSpec * pFieldOptions;
	// The number of field options, used mainly for checking/debug purposes
	unsigned int uFieldOptionCount;

	// The current token chain
	CXXTokenChain * pTokenChain;

	// The last template we found
	CXXTokenChain * pTemplateTokenChain;

	// The last token we have extracted. This is always pushed to
	// the token chain tail (which will take care of deletion)
	CXXToken * pToken; // the token chain tail

	// The last char we have extracted from input
	int iChar;

	// Toplevel keyword state. A combination of CXXParserKeywordState flags.
	// Please note that the keywords appearing inside a () subchain are NOT marked.
	unsigned int uKeywordState;

	// This is used to handle the special case of "final" which is a keyword
	// in class/struct/union declarations but not anywhere else
	bool bParsingClassStructOrUnionDeclaration;
	
	// public, protected and private keywords are C++ only.
	// However when parsing .h files we don't know if they belong to
	// a C program or C++ one and thus for safety we parse them as C++.
	// If our guess is wrong then we might discard certain variable
	// declarations thinking that they're composed of keywords.
	//
	// For public, protected, and private keywords we can try the following trick.
	//
	// In header files we disable processing of such keywords until we either figure
	// out that the file really contains C++ or we start parsing a struct/union.
	//
	// This flag is meaningful only when parsing a .h file as C++ since in C
	// public/protected/private are never keywords and we assume that .cpp files
	// have C++ content (so public/protected/private are always keywords).
	bool bEnablePublicProtectedPrivateKeywords;

} CXXParserState;



// defined in cxx_parser.c
extern CXXParserState g_cxx;

#define cxxParserCurrentLanguageIsCPP() \
	(g_cxx.eLanguage == g_cxx.eCPPLanguage)

#define cxxParserCurrentLanguageIsC() \
	(g_cxx.eLanguage == g_cxx.eCLanguage)

#endif //!ctags_cxx_parser_internal_h_
