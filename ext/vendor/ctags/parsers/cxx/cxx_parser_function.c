/*
*   Copyright (c) 2016, Szymon Tomasz Stefanek
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for parsing and scanning C++ source files
*/
#include "cxx_parser.h"
#include "cxx_parser_internal.h"

#include "cxx_debug.h"
#include "cxx_keyword.h"
#include "cxx_token.h"
#include "cxx_token_chain.h"
#include "cxx_scope.h"

#include "parse.h"
#include "vstring.h"
#include "lcpp.h"
#include "debug.h"
#include "keyword.h"
#include "read.h"

#include <string.h>

//
// This is called upon encountering a semicolon, when current language is
// C and we are in global scope.
//
// Try to handle the special case of C K&R style function declarations.
// Returns -1 in case of error, 1 if a K&R style function declaration has been
// found and parsed, 0 if no K&R style function declaration has been found.
//
int cxxParserMaybeExtractKnRStyleFunctionDefinition(int * piCorkQueueIndex)
{
#ifdef CXX_DO_DEBUGGING
	vString * pChain = cxxTokenChainJoin(g_cxx.pTokenChain,NULL,0);
	CXX_DEBUG_PRINT(
			"Looking for K&R-style function in '%s'",
			vStringValue(pChain)
		);
	vStringDelete(pChain);
#endif

	if(piCorkQueueIndex)
		*piCorkQueueIndex = CORK_NIL;

	// Check if we are in the following situation:
	//
	//   type1 function(arg1,arg2,...) type2 arg1; type3 arg2; {
	//                                           ^
	//                                       we're here

	CXX_DEBUG_ASSERT(
			g_cxx.eLanguage == g_cxx.eCLanguage,
			"Should be called only when parsing C"
		);
	CXX_DEBUG_ASSERT(
			cxxTokenChainLast(g_cxx.pTokenChain),
			"At least one token should be there"
		);
	CXX_DEBUG_ASSERT(
			cxxTokenTypeIs(cxxTokenChainLast(g_cxx.pTokenChain),CXXTokenTypeSemicolon),
			"Only upon encountering a semicolon"
		);

	// The minimum possible case is:
	//
	//   func(arg) type2 arg;
	//
	// where (arg) is a condensed parenthesis chain.
	// So the minimum number of tokens required is 5: func, (arg), type2, arg, ;
	if(g_cxx.pTokenChain->iCount < 5)
		return 0; // no way

	// There must be a parenthesis chain
	CXXToken * pParenthesis = cxxTokenChainFirstTokenOfType(
			g_cxx.pTokenChain,
			CXXTokenTypeParenthesisChain
		);
	if(!pParenthesis)
		return 0; // no parenthesis chain

	// The parenthesis chain must have an identifier before it
	CXXToken * pIdentifier = pParenthesis->pPrev;
	if(!pIdentifier)
		return 0;
	if(!cxxTokenTypeIs(pIdentifier,CXXTokenTypeIdentifier))
		return 0;

	// And least three tokens after it
	CXXToken * x = pParenthesis->pNext;
	if(!x)
		return 0;
	x = x->pNext;
	if(!x)
		return 0;
	x = x->pNext;
	if(!x)
		return 0;

	// The tokens following must be only things allowed in a variable declaration
	x = cxxTokenChainNextTokenNotOfType(
			pParenthesis,
			CXXTokenTypeIdentifier | CXXTokenTypeKeyword |
				CXXTokenTypeSquareParenthesisChain | CXXTokenTypeStar |
				CXXTokenTypeComma | CXXTokenTypeSingleColon | CXXTokenTypeNumber
		);

	CXX_DEBUG_ASSERT(x,"There should be at least the terminator here!");
	if(!x)
		return 0;

	if(!cxxTokenTypeIs(x,CXXTokenTypeSemicolon))
		return 0; // does not look like a variable declaration.

	x = cxxTokenChainPreviousTokenNotOfType(
			x,
			CXXTokenTypeSquareParenthesisChain | CXXTokenTypeSingleColon |
				CXXTokenTypeNumber
		);

	CXX_DEBUG_ASSERT(x,"We should have found an identifier here");
	if(!x)
		return 0;

	if(!cxxTokenTypeIs(x,CXXTokenTypeIdentifier))
		return 0; // does not look like a variable declaration.

	CXX_DEBUG_ASSERT(
			pParenthesis->pChain,
			"The parenthesis should be condensed here!"
		);

	CXXTokenChain * pParenthesisTokenChain = g_cxx.pTokenChain;

	CXXToken * pFirstArgumentToken = pParenthesis->pNext;

	// Special case inside special case.
	// Check if we're at something like func __ARGS(())
	if(
			(pParenthesis->pChain->iCount == 3) &&
			cxxTokenTypeIs(
					cxxTokenChainAt(pParenthesis->pChain,1),
					CXXTokenTypeParenthesisChain
				) &&
			(pIdentifier->pPrev) &&
			cxxTokenTypeIs(pIdentifier->pPrev,CXXTokenTypeIdentifier)
		)
	{
		// Looks exactly like our special case.
		pIdentifier = pIdentifier->pPrev;
		pParenthesisTokenChain = pParenthesis->pChain;
		pParenthesis = cxxTokenChainAt(pParenthesis->pChain,1);
	}

	// Now check if the contents of the parenthesis chain look like a K&R signature

	// This is something like identifier,identifier,identifier,...
	if(pParenthesis->pChain->iCount < 3)
		return 0; // no way

	x = pParenthesis->pChain->pHead->pNext;
	CXX_DEBUG_ASSERT(x,"We should have found something in the parenthesis chain");

	int iParameterCount = 0;
	bool bGotMultipleDots = false;

	for(;;)
	{
		if(cxxTokenTypeIs(x,CXXTokenTypeIdentifier))
			iParameterCount++;
		else if(cxxTokenTypeIs(x,CXXTokenTypeMultipleDots))
			bGotMultipleDots = true;
		else {
			// not valid (note that (void) is not allowed here since we
			// wouln't have a following variable declaration)
			return 0;
		}

		x = x->pNext;
		CXX_DEBUG_ASSERT(x,"We should have found at least the closing parenthesis");
		if(cxxTokenTypeIs(x,CXXTokenTypeClosingParenthesis))
			break;
		if(bGotMultipleDots)
			return 0; // not valid
		if(!cxxTokenTypeIs(x,CXXTokenTypeComma))
			return 0;
		x = x->pNext;
		CXX_DEBUG_ASSERT(x,"We should have found at least the closing parenthesis");
	}

	if(iParameterCount < 1)
	{
		// we should have found at least one paremeter
		// (the one that we found before the ;)
		return 0;
	}

	cxxTokenChainTake(g_cxx.pTokenChain,pIdentifier);
	cxxTokenChainTake(pParenthesisTokenChain,pParenthesis);

	// remove the whole signature from the chain
	while(g_cxx.pTokenChain->pHead != pFirstArgumentToken)
		cxxTokenChainDestroyFirst(g_cxx.pTokenChain);

	CXX_DEBUG_ASSERT(
			g_cxx.pTokenChain->pHead,
			"We should have the variable declaration in the chain now!"
		);

	// There is exactly one statement in chain now.

	// Extra here means "following the first"
#define MAX_EXTRA_KNR_PARAMETERS 10

	CXXToken * aExtraParameterStarts[MAX_EXTRA_KNR_PARAMETERS];
	int iExtraStatementsInChain = 0;

	// Now we should have no more than iParameterCount-1 parameters before
	// an opening bracket. There may be less declarations as each one may
	// declare multiple variables and C89 supports the implicit "int" type rule.
	// Note that we parse up to iParameterCount statements (which will be lost
	// if we can't find an opening bracket).
	while(iParameterCount > 0)
	{
		CXXToken * pCurrentTail = g_cxx.pTokenChain->pTail;

		if(!cxxParserParseUpToOneOf(
				CXXTokenTypeSemicolon | CXXTokenTypeOpeningBracket | CXXTokenTypeEOF
			))
		{
			cxxTokenDestroy(pIdentifier);
			cxxTokenDestroy(pParenthesis);
			return -1;
		}

		if(cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeEOF))
		{
			cxxTokenDestroy(pIdentifier);
			cxxTokenDestroy(pParenthesis);
			return 0; // tolerate syntax error
		}

		if(iExtraStatementsInChain < MAX_EXTRA_KNR_PARAMETERS)
		{
			CXX_DEBUG_ASSERT(
					pCurrentTail->pNext,
					"We should have parsed an additional statement here"
				);
			aExtraParameterStarts[iExtraStatementsInChain] = pCurrentTail->pNext;
			iExtraStatementsInChain++;
		}

		if(cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket))
			break; // gotcha

		iParameterCount--;
	}

	if(!cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket))
	{
		cxxTokenDestroy(pIdentifier);
		cxxTokenDestroy(pParenthesis);
		// Didn't find an opening bracket.
		// This probably wasn't a K&R style function declaration after all.
		return 0;
	}

	tagEntryInfo * tag = cxxTagBegin(CXXTagKindFUNCTION,pIdentifier);

	if(tag)
	{
		if(pParenthesis->pChain->pTail)
		{
			// normalize signature
			cxxTokenChainNormalizeTypeNameSpacing(pParenthesis->pChain);
			// make sure we don't emit the trailing space
			pParenthesis->pChain->pTail->bFollowedBySpace = false;
		}

		tag->isFileScope = (g_cxx.uKeywordState & CXXParserKeywordStateSeenStatic) &&
				!isInputHeaderFile();

		vString * pszSignature = cxxTokenChainJoin(pParenthesis->pChain,NULL,0);

		// FIXME: Return type!
		// FIXME: Properties?

		if(pszSignature)
			tag->extensionFields.signature = vStringValue(pszSignature);

		int iCorkQueueIndex = cxxTagCommit();

		if(piCorkQueueIndex)
			*piCorkQueueIndex = iCorkQueueIndex;

		if(pszSignature)
			vStringDelete(pszSignature);
	}

	cxxTokenDestroy(pParenthesis);

	CXX_DEBUG_PRINT(
			"Found K&R-style function '%s'",
			vStringValue(pIdentifier->pszWord)
		);

	cxxScopePush(pIdentifier,CXXScopeTypeFunction,CXXScopeAccessUnknown);

	// emit parameters
	if(cxxTagKindEnabled(CXXTagKindPARAMETER))
	{
		// The chain contains 1 + iExtraStatementsInChain statements now
		int iIdx = 0;
		for(;;)
		{
			cxxParserExtractVariableDeclarations(
					g_cxx.pTokenChain,
					CXXExtractVariableDeclarationsKnRStyleParameters
				);
			if(iIdx >= iExtraStatementsInChain)
				break;

			// kill everything up to the next start
			while(g_cxx.pTokenChain->pHead != aExtraParameterStarts[iIdx])
				cxxTokenChainDestroyFirst(g_cxx.pTokenChain);

			iIdx++;
		}
	}

	return 1;
}

//
// This function attempts to verify that the specified chain _looks like_
// a set of parameters to a function call. It's quite fuzzy and thus not
// 100% accurate, but it tries to exclude the obvious cases. If it says
// "no" then the specified chain CAN'T be a set of parameters to
// a function call. If it says "yes" then the result has to be considered
// a guess: the chain *might* be a set of parameters to a functino call.
//
// This function is used to check both () and {} parenthesis chains.
//     function(...)
//     variable(...)
//     variable{...}
//
bool cxxParserTokenChainLooksLikeFunctionCallParameterSet(
		CXXTokenChain * pChain
	)
{
	CXXToken * t = pChain->pHead;
	CXXToken * pLast = pChain->pTail;

	CXX_DEBUG_ASSERT(
			cxxTokenTypeIsOneOf(t,CXXTokenTypeOpeningParenthesis | CXXTokenTypeOpeningBracket),
			"The token chain should start with an opening parenthesis/bracket"
		);
	CXX_DEBUG_ASSERT(
			cxxTokenTypeIsOneOf(pLast,CXXTokenTypeClosingParenthesis | CXXTokenTypeClosingBracket),
			"The token chain should end with an closing parenthesis/bracket"
		);

	unsigned int uTerminator = t->eType << 4;

	// Dealing with (...) type chain and not {...} one
	bool bDealingWithParenthesisChain = (uTerminator == CXXTokenTypeClosingParenthesis);

	t = t->pNext;

	while(t != pLast)
	{
		if(
			bDealingWithParenthesisChain &&
			cxxTokenTypeIsOneOf(t,
				CXXTokenTypeNumber | CXXTokenTypeStringConstant |
				CXXTokenTypeCharacterConstant | CXXTokenTypePointerOperator |
				CXXTokenTypeDotOperator | CXXTokenTypeOperator | CXXTokenTypeMultipleDots
			))
		{
			// Not allowed in a function signature before an equal sign (which
			// we haven't encountered yet).
			// assume this looks like a function call
			return true;
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeKeyword))
		{
			if(cxxKeywordMayBePartOfTypeName(t->eKeyword))
			{
				// parts of type name (not inside a parenthesis
				// which is assumed to be condensed)
				return false;
			}
			
			if(
				bDealingWithParenthesisChain &&
				(
					cxxKeywordIsConstant(t->eKeyword) ||
					(t->eKeyword == CXXKeywordNEW)
				)
			)
			{
				// Not allowed in a function signature before an equal sign (which
				// we haven't encountered yet).
				// assume this looks like a function call
				return true;
			}
			
			if(
					(t->eKeyword != CXXKeywordNEW) &&
					cxxTokenTypeIsOneOf(
							t->pNext,
							CXXTokenTypeKeyword | CXXTokenTypeStar | CXXTokenTypeAnd |
								CXXTokenTypeMultipleAnds | CXXTokenTypeIdentifier
						)
				)
			{
				// this is something like:
				// (int a...
				// (void *...
				// (unsigned int...
				return false;
			}

		} else if(cxxTokenTypeIs(t,CXXTokenTypeIdentifier))
		{
			if(cxxTokenTypeIsOneOf(t->pNext,CXXTokenTypeKeyword | CXXTokenTypeIdentifier))
			{
				// this is something like:
				// (a b...
				return false;
			}
		} else if(cxxTokenTypeIs(t,CXXTokenTypeGreaterThanSign))
		{
			if(cxxTokenTypeIsOneOf(
					t->pNext,
					CXXTokenTypeAnd | CXXTokenTypeStar |
						CXXTokenTypeMultipleAnds | CXXTokenTypeComma | uTerminator
				))
			{
				// > &
				// > *
				// > &&
				// >,
				// >) or >}
				return false;
			}

			if(cxxTokenTypeIsOneOf(t->pPrev,CXXTokenTypeKeyword))
			{
				// int>
				//
				return false;
			}
		} else if(
				cxxTokenTypeIs(t,CXXTokenTypeParenthesisChain) &&
				cxxTokenTypeIsOneOf(
						t->pPrev,
						CXXTokenTypeIdentifier | CXXTokenTypeKeyword |
							CXXTokenTypeStar | CXXTokenTypeAnd | CXXTokenTypeGreaterThanSign
					) &&
				cxxTokenTypeIs(t->pNext,CXXTokenTypeParenthesisChain) &&
				cxxTokenTypeIs(cxxTokenChainAt(t->pChain,1),CXXTokenTypeStar) &&
				cxxParserTokenChainLooksLikeFunctionParameterList(t->pNext->pChain,NULL)
			)
		{
			// looks like a function pointer
			//   someType (*p)(int)
			return false;
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeAssignment))
		{
			// after an assignment prototypes and constructor
			// declarations may look the same, skip to next comma or end
			t = cxxTokenChainNextTokenOfType(
					t,
					uTerminator | CXXTokenTypeComma
				);
			CXX_DEBUG_ASSERT(t,"We should have found the terminator here!");
			if(cxxTokenTypeIs(t,CXXTokenTypeComma))
				t = t->pNext;
		} else {
			t = t->pNext;
		}
	}

	// We must assume that it might be...
	return true;
}

//
// Try to tell if the specified token chain is valid as a parameter list
// for a constructor. It's used to check if something like type name(args)
// belongs to a variable declaration.
//
// This is more of a guess for now: tries to exclude trivial cases.
//
bool cxxParserTokenChainLooksLikeConstructorParameterSet(
		CXXTokenChain * pChain
	)
{
	// We assume that the chain has a starting parenthesis and an
	// ending parenthesis.

	if(pChain->iCount < 3)
	{
		CXX_DEBUG_ASSERT(
				pChain->iCount == 2,
				"This function should be called only on parenthesis and bracket chains"
			);

		if(cxxTokenTypeIs(cxxTokenChainFirst(pChain),CXXTokenTypeOpeningBracket))
		{
			CXX_DEBUG_ASSERT(
					cxxTokenTypeIs(cxxTokenChainLast(pChain),CXXTokenTypeClosingBracket),
					"The last token should have been a closing bracket here"
				);
			return true; // type var {} is valid in C++11
		}

		CXX_DEBUG_ASSERT(
				cxxTokenTypeIs(cxxTokenChainFirst(pChain),CXXTokenTypeOpeningParenthesis),
				"This function should be called only on parenthesis and bracket chains"
			);

		return false; // type var() is NOT valid C++
	}

	return cxxParserTokenChainLooksLikeFunctionCallParameterSet(pChain);
}

//
// Look for a function signature in the specified chain.
//
// If a proper function signature is found then also standardize the spacing
// of the identifier so we always get it as "operator ()" and never as
// "operator() or operator ( ) ".
//
// Note that this function does NOT handle the special case of K&R-style
// declarations.
//
// If pParamInfo is not null, it is passed to
// cxxParserTokenChainLooksLikeFunctionParameterList() which will eventually
// fill it up.
//
//
bool cxxParserLookForFunctionSignature(
		CXXTokenChain * pChain,
		CXXFunctionSignatureInfo * pInfo,
		CXXFunctionParameterInfo * pParamInfo
	)
{
	CXX_DEBUG_ENTER();

	if(pChain->iCount < 1)
	{
		CXX_DEBUG_LEAVE_TEXT("Chain is empty");
		return false;
	}

#ifdef CXX_DO_DEBUGGING
	vString * pJoinedChain = cxxTokenChainJoin(pChain,NULL,0);
	CXX_DEBUG_PRINT(
			"Looking for function signature in '%s'",
			vStringValue(pJoinedChain)
		);
	vStringDelete(pJoinedChain);
#endif

	if(pParamInfo)
		pParamInfo->uParameterCount = 0;

	CXX_DEBUG_ASSERT(pChain,"Null chain");

	CXXToken * pToken = cxxTokenChainFirst(pChain);

	pInfo->uFlags = 0;
	pInfo->pParenthesis = NULL;
	pInfo->pTrailingComma = NULL;

	CXXToken * pIdentifierStart = NULL;
	CXXToken * pIdentifierEnd = NULL;
	CXXToken * pTopLevelParenthesis = NULL;

	while(pToken)
	{
		CXX_DEBUG_PRINT(
				"Token '%s' of type 0x%02x",
				vStringValue(pToken->pszWord),
				pToken->eType
			);

		if(cxxTokenTypeIsOneOf(
				pToken,
				CXXTokenTypeOpeningBracket | CXXTokenTypeSemicolon | CXXTokenTypeEOF
			))
		{
			// reached end
			break;
		}
		
		if(cxxTokenTypeIs(pToken,CXXTokenTypeComma))
		{
			// reached end, but we have a trailing comma.
			pInfo->pTrailingComma = pToken;
			break;
		}

		if(
			cxxParserCurrentLanguageIsCPP() &&
			cxxTokenTypeIsOneOf(
					pToken,
					CXXTokenTypeSingleColon | CXXTokenTypeAssignment |
						CXXTokenTypePointerOperator
				)
		)
		{
			// With a single colon it might be a constructor.
			// With assignment it might be virtual type func(..) = 0;
			// With a pointer operator it might be trailing return type
			break;
		} 

		if(cxxTokenTypeIsOneOf(
					pToken,
					CXXTokenTypeOperator | CXXTokenTypePointerOperator |
						CXXTokenTypeBracketChain | CXXTokenTypeStringConstant |
						CXXTokenTypeCharacterConstant | CXXTokenTypeMultipleDots |
						CXXTokenTypeClosingBracket | CXXTokenTypeClosingParenthesis |
						CXXTokenTypeClosingSquareParenthesis
				)
			)
		{
			// Nope.
			CXX_DEBUG_LEAVE_TEXT("Found tokens that should not appear in a function signature");
			return false;
		}

		if(cxxTokenTypeIs(pToken,CXXTokenTypeSmallerThanSign))
		{
			pToken = cxxTokenChainSkipToEndOfTemplateAngleBracket(pToken);
			if(!pToken)
			{
				CXX_DEBUG_LEAVE_TEXT("Couln't skip past angle bracket chain");
				return false;
			}
			goto next_token;
		}

		// Didn't reach end. Check if we have a parenthesis.

		CXXTokenChain * pIdentifierChain = pChain;
		CXXToken * pParenthesis = NULL;

		if(cxxTokenIsKeyword(pToken,CXXKeywordOPERATOR))
		{
			// Special case for operator <something> ()

			pIdentifierStart = pToken;
			pToken = pToken->pNext;

			while(pToken)
			{
				if(cxxTokenTypeIs(pToken,CXXTokenTypeParenthesisChain))
				{
					// check for operator ()()
					if(
							pToken->pNext &&
							cxxTokenTypeIs(pToken->pNext,CXXTokenTypeParenthesisChain)
						)
						pToken = pToken->pNext;

					break;
				} else if(cxxTokenTypeIs(pToken,CXXTokenTypeKeyword))
				{
					if(
							(!cxxTokenIsKeyword(pToken,CXXKeywordNEW)) &&
							(!cxxTokenIsKeyword(pToken,CXXKeywordDELETE))
						)
					{
						CXX_DEBUG_LEAVE_TEXT("Unexpected token after the operator keyword");
						return false;
					}
				} else if(!cxxTokenTypeIsOneOf(
						pToken,
						CXXTokenTypeAnd | CXXTokenTypeAssignment |
							CXXTokenTypeComma | CXXTokenTypeDotOperator |
							CXXTokenTypeAngleBracketChain |
							CXXTokenTypeGreaterThanSign | CXXTokenTypeOperator |
							CXXTokenTypePointerOperator | CXXTokenTypeSingleColon |
							CXXTokenTypeSmallerThanSign | CXXTokenTypeSquareParenthesisChain |
							CXXTokenTypeStar | CXXTokenTypeMultipleAnds
					)
				)
				{
					CXX_DEBUG_LEAVE_TEXT("Unexpected token after the operator keyword");
					return false;
				}

				pToken = pToken->pNext;
			}

			if(!pToken)
			{
				CXX_DEBUG_LEAVE_TEXT("Didn't find a parenthesis subchain after operator keyword");
				return false;
			}

			CXX_DEBUG_ASSERT(
					cxxTokenTypeIs(pToken,CXXTokenTypeParenthesisChain),
					"Must have found a parenthesis chain here"
				);

			pParenthesis = pToken;
			pIdentifierEnd = pToken->pPrev;

		} else {

			// must be parenthesis chain
			if(!cxxTokenTypeIs(pToken,CXXTokenTypeParenthesisChain))
				goto next_token;

			// parentheses at position 0 are meaningless
			if(!pToken->pPrev)
				goto next_token;

			CXX_DEBUG_PRINT("Found parenthesis chain: check for identifier");

			// parentheses at position 1 they are likely to be macro invocations...
			// but we still handle them in case we find nothing else.

			// must have an identifier before (this excludes things like __attribute__
			// and declspec which are marked as keywords)
			
			pParenthesis = pToken;
			
			if(cxxTokenTypeIs(pToken->pPrev,CXXTokenTypeIdentifier))
			{
				// identifier before
				CXX_DEBUG_PRINT("Got identifier before");
				pIdentifierStart = pToken->pPrev;
				pIdentifierEnd = pToken->pPrev;
			} else if(
				// check for complex parenthesized declarations.
				// Keep functions, discard everything else.
				//
				// Possible cases:
				//    ret type (*baz)(params) <-- function pointer (variable)
				//    ret type (*(baz))(params) <-- function pointer (variable)
				//    ret type (* const (baz))(params) <-- function pointer (variable)
				//    ret type (*baz())() <-- function returning function pointer
				//    ret type (*baz(params)) <-- function returning a pointer
				//    ret type (*baz(params))[2] <-- function returning a pointer to array

#if 0
				// Probably not needed
				(
					// Followed by another parenthesis that looks like a function signature
					(
						pToken->pNext &&
						cxxTokenTypeIs(pToken->pNext,CXXTokenTypeParenthesisChain) &&
						// the next parenthesis must look like a function parameter list (return function)
						cxxParserTokenChainLooksLikeFunctionParameterList(
								pToken->pNext->pChain,
								NULL
							)
					) ||
					(
						pToken->pNext &&
						cxxTokenTypeIs(pToken->pNext,CXXTokenTypeSquareParenthesisChain) &&
					)
				) &&
#endif
				// look for the identifier
				(pIdentifierStart = cxxTokenChainFirstPossiblyNestedTokenOfType(
						pToken->pChain,
						CXXTokenTypeIdentifier,
						&pIdentifierChain
					))
				)
			{
				// Now pIdentifierStart points at the innermost identifier
				// Check if it's followed by a parameter list
				if(
					pIdentifierStart->pNext &&
					cxxTokenTypeIs(pIdentifierStart->pNext,CXXTokenTypeParenthesisChain) &&
					cxxParserTokenChainLooksLikeFunctionParameterList(
							pIdentifierStart->pNext->pChain,
							NULL
						)
					)
				{
					CXX_DEBUG_PRINT("Still looking like a function");
					pIdentifierEnd = pIdentifierStart;
					// correct our guess for parenthesis
					pParenthesis = pIdentifierStart->pNext;
				} else {
					// Looks more like a function pointer or something else we can't figure out
					CXX_DEBUG_LEAVE_TEXT(
							"Does not look like a function (possibly pointer or something else)"
						);
					return false;
				}
			} else if(
					pToken->pPrev->pPrev &&
					cxxTokenTypeIs(pToken->pPrev,CXXTokenTypeGreaterThanSign)
				)
			{
				// look for template specialisation
				CXXToken * pSpecBegin = cxxTokenChainSkipBackToStartOfTemplateAngleBracket(
						pToken->pPrev
					);

				if(
						pSpecBegin &&
						pSpecBegin->pPrev &&
						cxxTokenTypeIs(pSpecBegin->pPrev,CXXTokenTypeIdentifier)
					)
				{
					// template specialisation
					CXX_DEBUG_PRINT("Maybe template specialisation?");
					pIdentifierStart = pSpecBegin->pPrev;
					pIdentifierEnd = pSpecBegin->pPrev;
					pInfo->uFlags |= CXXFunctionSignatureInfoTemplateSpecialization;
				} else {
					// no way
					goto next_token;
				}
			} else {
				// no way
				goto next_token;
			}
		}

		if(pParenthesis)
		{
			CXX_DEBUG_ASSERT(
					cxxTokenTypeIs(pParenthesis,CXXTokenTypeParenthesisChain),
					"Must have found a parenthesis chain here"
				);
	
			// looks almost fine
	
			CXXToken * pInner = cxxTokenChainAt(pParenthesis->pChain,1);
	
			// Look for the __ARGS(()) macro pattern.
			if(
					// nested parentheses
					(pParenthesis->pChain->iCount == 3) &&
					cxxTokenTypeIs(pInner,CXXTokenTypeParenthesisChain) &&
					// FIXME: This actually excludes operator!
					cxxTokenTypeIs(pIdentifierEnd,CXXTokenTypeIdentifier) &&
					// an identifier right before the identifier we found
					pIdentifierEnd->pPrev &&
					cxxTokenTypeIs(pIdentifierEnd->pPrev,CXXTokenTypeIdentifier) &&
					cxxParserTokenChainLooksLikeFunctionParameterList(
							pInner->pChain,
							pParamInfo
						)
				)
			{
				// __ARGS() case
				pTopLevelParenthesis = pParenthesis;
				pInfo->pParenthesisContainerChain = pParenthesis->pChain;
				pInfo->pIdentifierEnd = pIdentifierEnd->pPrev;
				pInfo->pIdentifierStart = pInfo->pIdentifierEnd;
				pInfo->pIdentifierChain = pIdentifierChain;
				pInfo->pParenthesis = pInner;
			} else if(cxxParserTokenChainLooksLikeFunctionParameterList(
					pParenthesis->pChain,
					pParamInfo
				))
			{
				// non __ARGS()
				pTopLevelParenthesis = pParenthesis;
				pInfo->pParenthesisContainerChain = pIdentifierChain;
				pInfo->pIdentifierStart = pIdentifierStart;
				pInfo->pIdentifierEnd = pIdentifierEnd;
				pInfo->pIdentifierChain = pIdentifierChain;
				pInfo->pParenthesis = pParenthesis;
			}
		}

next_token:
		pToken = pToken->pNext;
	}

	if(!pInfo->pParenthesis)
	{
		CXX_DEBUG_LEAVE_TEXT("No suitable parenthesis chain found");
		return false; // no function, no party
	}

	if(pInfo->pIdentifierStart != pInfo->pIdentifierEnd)
	{
		// operator case
		pInfo->pIdentifierStart->bFollowedBySpace = true; // force proper spacing
		CXXToken * t = pInfo->pIdentifierStart->pNext;
		while(t != pInfo->pIdentifierEnd)
		{
			t->bFollowedBySpace = false;
			t = t->pNext;
		}
	} else {
		// non operator
		pInfo->pIdentifierStart->bFollowedBySpace = false; // force proper spacing
	}

	pInfo->pIdentifierEnd->bFollowedBySpace = false; // force proper spacing

	pInfo->pScopeStart = NULL;

	if(cxxParserCurrentLanguageIsCPP())
	{
		// Look for scope prefix
		CXXToken * pAux = pInfo->pIdentifierStart->pPrev;

		CXX_DEBUG_PRINT("Looking for scope prefix");

		while(pAux)
		{
			CXX_DEBUG_PRINT(
					"Token '%s' of type 0x%02x",
					vStringValue(pAux->pszWord),
					pAux->eType
				);

			if(!cxxTokenTypeIs(pAux,CXXTokenTypeMultipleColons))
				break;
			pAux = pAux->pPrev;
			if(!pAux)
				break;
			if(!cxxTokenTypeIs(pAux,CXXTokenTypeIdentifier))
			{
				// check for template specialization
				if(cxxTokenTypeIs(pAux,CXXTokenTypeGreaterThanSign))
				{
					// might be something like type X<TemplateArg>::func()
					// (explicit specialization of template<A> class X).
					CXXToken * pSmallerThan = cxxTokenChainSkipBackToStartOfTemplateAngleBracket(
							pAux
						);
					if(!pSmallerThan)
						break; // nope
					if(!pSmallerThan->pPrev)
						break; // nope
					if(!cxxTokenTypeIs(pSmallerThan->pPrev,CXXTokenTypeIdentifier))
						break; // nope
					// hmm.. probably a template specialisation
					pAux = pSmallerThan->pPrev;
					pInfo->uFlags |= CXXFunctionSignatureInfoScopeTemplateSpecialization;
				} else if(pAux->eType == CXXTokenTypeAngleBracketChain)
				{
					// same as above, but already condensed (though it should never happen)
					if(!pAux->pPrev)
						break; // nope
					if(!cxxTokenTypeIs(pAux->pPrev,CXXTokenTypeIdentifier))
						break; // nope
					// hmm.. probably a template specialisation
					pAux = pAux->pPrev;
					pInfo->uFlags |= CXXFunctionSignatureInfoScopeTemplateSpecialization;
				} else {
					// no more scope names
					break;
				}
			}

			CXX_DEBUG_PRINT("Shifting scope start to '%s'",vStringValue(pAux->pszWord));

			pInfo->pScopeStart = pAux;

			pAux = pAux->pPrev;
		}

		CXX_DEBUG_PRINT("Scope prefix search finished");

		// Look for trailing const.

		if(pTopLevelParenthesis->pNext)
		{
			if(cxxTokenIsKeyword(pTopLevelParenthesis->pNext,CXXKeywordCONST))
				pInfo->pSignatureConst = pTopLevelParenthesis->pNext;
			else
				pInfo->pSignatureConst = NULL;

			// Look for = 0 for "pure" modifier
			CXXToken * pAssignment = cxxTokenChainNextTokenOfType(
					pTopLevelParenthesis,
					CXXTokenTypeAssignment
				);

			if(pAssignment && pAssignment->pNext)
			{
				if(
					cxxTokenTypeIs(pAssignment->pNext,CXXTokenTypeNumber) &&
					(strcmp(vStringValue(pAssignment->pNext->pszWord),"0") == 0)
				)
					pInfo->uFlags |= CXXFunctionSignatureInfoPure;
				else if(cxxTokenTypeIs(pAssignment->pNext,CXXTokenTypeKeyword))
				{
					if(pAssignment->pNext->eKeyword == CXXKeywordDEFAULT)
						pInfo->uFlags |= CXXFunctionSignatureInfoDefault;
					if(pAssignment->pNext->eKeyword == CXXKeywordDELETE)
						pInfo->uFlags |= CXXFunctionSignatureInfoDelete;
				}
			}

			CXXToken * pIdentOrKeyword = cxxTokenChainNextTokenOfType(
					pTopLevelParenthesis,
					CXXTokenTypeIdentifier | CXXTokenTypeKeyword
				);

			while(pIdentOrKeyword)
			{
				// override is a keyword only in specific contexts so we handle it as identifier
				if(cxxTokenTypeIs(pIdentOrKeyword,CXXTokenTypeKeyword))
				{
					if(pIdentOrKeyword->eKeyword == CXXKeywordVOLATILE)
						pInfo->uFlags |= CXXFunctionSignatureInfoVolatile;
				} else {
					// The "final" keyword is actually disabled in most contexts so we handle
					// it as identifier. "override is always handled as identifier.
					if(strcmp(vStringValue(pIdentOrKeyword->pszWord),"final") == 0)
						pInfo->uFlags |= CXXFunctionSignatureInfoFinal;
					else if(strcmp(vStringValue(pIdentOrKeyword->pszWord),"override") == 0)
						pInfo->uFlags |= CXXFunctionSignatureInfoOverride;
				}

				pIdentOrKeyword = cxxTokenChainNextTokenOfType(
						pIdentOrKeyword,
						CXXTokenTypeIdentifier | CXXTokenTypeKeyword
					);
			}
		} else {
			pInfo->pSignatureConst = NULL;
		}
	} else {
		pInfo->pSignatureConst = NULL;
	}

	// Check return type
	if(pInfo->pIdentifierChain != pChain)
	{
		// Nested parentheses. In this case the type name is the whole chain (excluding
		// the identifier and the signature).
		CXX_DEBUG_PRINT("Nested parentheses, probably a function returning pointers");
		pInfo->pTypeStart = cxxTokenChainFirst(pChain);
		pInfo->pTypeEnd = pToken ? pToken->pPrev : cxxTokenChainLast(pChain);
		pInfo->bTypeContainsIdentifierScopeAndSignature = true;
	} else {
		pToken = pInfo->pScopeStart ? pInfo->pScopeStart : pInfo->pIdentifierStart;
		
		if(pToken->pPrev)
		{
			CXXToken * pParenthesisOrConst = pInfo->pSignatureConst ?
					pInfo->pSignatureConst : pInfo->pParenthesis;
			if(
					cxxParserCurrentLanguageIsCPP() &&
					cxxTokenTypeIs(pToken->pPrev,CXXTokenTypeKeyword) &&
					(pToken->pPrev->eKeyword == CXXKeywordAUTO) &&
					pParenthesisOrConst->pNext &&
					cxxTokenTypeIs(
							pParenthesisOrConst->pNext,
							CXXTokenTypePointerOperator
						) &&
					pParenthesisOrConst->pNext->pNext &&
					(!cxxTokenTypeIsOneOf(
							pParenthesisOrConst->pNext->pNext,
							CXXTokenTypeSemicolon | CXXTokenTypeOpeningBracket
						))
				)
			{
				// looks like trailing return type
				//   auto f() -> int;
				//   auto f() -> int {
				pInfo->pTypeStart = pParenthesisOrConst->pNext->pNext;
				pInfo->pTypeEnd = pInfo->pTypeStart;
				while(
					pInfo->pTypeEnd->pNext &&
					(!cxxTokenTypeIsOneOf(
							pInfo->pTypeEnd->pNext,
							CXXTokenTypeSemicolon | CXXTokenTypeOpeningBracket
						))
				)
					pInfo->pTypeEnd = pInfo->pTypeEnd->pNext;
			} else {
				// probaby normal return type
				pInfo->pTypeEnd = pToken->pPrev;
				pInfo->pTypeStart = cxxTokenChainFirst(pChain);
			}
		} else {
			pInfo->pTypeEnd = NULL;
			pInfo->pTypeStart = NULL;
		}
		pInfo->bTypeContainsIdentifierScopeAndSignature = false;
	}

	while(
			(pInfo->pTypeStart != pInfo->pTypeEnd) &&
			cxxTokenTypeIs(pInfo->pTypeStart,CXXTokenTypeKeyword) &&
			cxxKeywordExcludeFromTypeNames(pInfo->pTypeStart->eKeyword)
		)
		pInfo->pTypeStart = pInfo->pTypeStart->pNext;

	CXX_DEBUG_LEAVE_TEXT("Found function signature");
	return true;
}


//
// Emit a function tag.
//
// WARNING: This function is destructive: it removes the scope and
// identifier tokens from the chain. It will also move the parenthesis
// around (but will keep it as it contains the parameter definitions).
//
// Returns the number of scopes pushed if CXXEmitFunctionTagsPushScopes
// is present in uOptions and 0 otherwise.
//
int cxxParserEmitFunctionTags(
		CXXFunctionSignatureInfo * pInfo,
		unsigned int uTagKind,
		unsigned int uOptions,
		int * piCorkQueueIndex
	)
{
	CXX_DEBUG_ENTER();

	int iScopesPushed = 0;

	if(piCorkQueueIndex)
		*piCorkQueueIndex = CORK_NIL;

	enum CXXScopeType eOuterScopeType = cxxScopeGetType();

	bool bPushScopes = uOptions & CXXEmitFunctionTagsPushScopes;

	CXX_DEBUG_PRINT("Scope start is %x, push scope is %d",pInfo->pScopeStart,bPushScopes);

	// We'll be removing the scope and identifier, fix type
	if(
		(pInfo->pTypeStart == pInfo->pScopeStart) ||
		(pInfo->pTypeStart == pInfo->pIdentifierStart)
	)
		pInfo->pTypeStart = pInfo->pIdentifierEnd->pNext;
	
	CXX_DEBUG_ASSERT(pInfo->pTypeEnd != pInfo->pIdentifierEnd,"The type should never end at identifier");

	if(pInfo->pScopeStart)
	{
		if(bPushScopes)
		{
			CXX_DEBUG_PRINT("There is a scope and we're requested to push scopes");
	
			// there is a scope
			while(pInfo->pScopeStart != pInfo->pIdentifierStart)
			{
				CXXToken * pScopeId = pInfo->pScopeStart;
				
				pInfo->pScopeStart = cxxTokenChainNextTokenOfType(
						pInfo->pScopeStart,
						CXXTokenTypeMultipleColons
					);
				
				CXX_DEBUG_ASSERT(pInfo->pScopeStart,"We should have found a next token here");
				
				pInfo->pScopeStart = pInfo->pScopeStart->pNext;
				
				cxxTokenChainDestroyRange(
						pInfo->pIdentifierChain,
						pScopeId->pNext,
						pInfo->pScopeStart->pPrev
					);
	
				cxxTokenChainTake(pInfo->pIdentifierChain,pScopeId);
	
	
				CXX_DEBUG_PRINT("Pushing scope %s",vStringValue(pScopeId->pszWord));
	
				cxxScopePush(
						pScopeId,
						CXXScopeTypeClass,
						// WARNING: We don't know if it's really a class! (FIXME?)
						CXXScopeAccessUnknown
					);
				iScopesPushed++;
			}
		} else {
			cxxTokenChainDestroyRange(
					pInfo->pIdentifierChain,
					pInfo->pScopeStart,
					pInfo->pIdentifierStart->pPrev
				);
		}
	}

	CXXToken * pIdentifier = cxxTokenChainExtractRange(
			pInfo->pIdentifierStart,
			pInfo->pIdentifierEnd,
			// proper spacing has been already ensured
			// by cxxParserLookForFunctionSignature()
			0
		);

	cxxTokenChainDestroyRange(pInfo->pIdentifierChain,pInfo->pIdentifierStart,pInfo->pIdentifierEnd);

	CXX_DEBUG_ASSERT(
			pIdentifier,
			"The identifier should have been non null since the " \
				"indices point inside this chain"
		);
	pIdentifier->eType = CXXTokenTypeIdentifier; // force it

	CXX_DEBUG_PRINT("Identifier is '%s'",vStringValue(pIdentifier->pszWord));

	tagEntryInfo * tag = cxxTagBegin(uTagKind,pIdentifier);

	if(tag)
	{
		if(pInfo->pParenthesis->pChain->pTail)
		{
			// normalize signature
			cxxTokenChainNormalizeTypeNameSpacing(pInfo->pParenthesis->pChain);
			// make sure we don't emit the trailing space
			pInfo->pParenthesis->pChain->pTail->bFollowedBySpace = false;
		}

		if(uTagKind == CXXTagKindPROTOTYPE)
		{
			tag->isFileScope = !isInputHeaderFile();
		} else {
			// function definitions
			if(eOuterScopeType == CXXScopeTypeNamespace)
			{
				// in a namespace only static stuff declared in cpp files is file scoped
				tag->isFileScope = (
						g_cxx.uKeywordState & CXXParserKeywordStateSeenStatic
					) && (
						!isInputHeaderFile()
					);
			} else {
				// in a class/struct/union file scope stuff is only in cpp files
				tag->isFileScope = !isInputHeaderFile();
			}
		}

		vString * pszSignature = cxxTokenChainJoin(pInfo->pParenthesis->pChain,NULL,0);
		if(pInfo->pSignatureConst)
		{
			vStringCatS(pszSignature," ");
			cxxTokenAppendToString(pszSignature,pInfo->pSignatureConst);
		}

		CXXToken * pTypeName;

		if(pInfo->pTypeStart)
		{
			if(pInfo->bTypeContainsIdentifierScopeAndSignature)
			{
				CXX_DEBUG_PRINT("Type contains identifier and scope");
				// Special case: the type contains the identifier and parenthesis
				// (generally things like int (*foo(void))[2] or similar).
				
				// Scope and identifier have already been removed.
				// Remove the parenthesis, temporairly.
				if(pInfo->pTypeStart == pInfo->pParenthesis)
					pInfo->pTypeStart = pInfo->pParenthesis->pNext;
				if(pInfo->pTypeEnd == pInfo->pParenthesis)
					pInfo->pTypeEnd = pInfo->pParenthesis->pPrev;
				
				if(pInfo->pTypeStart && pInfo->pTypeEnd)
				{
					CXXToken * pTokenBeforeParenthesis = pInfo->pParenthesis->pPrev;
					cxxTokenChainTake(pInfo->pParenthesisContainerChain,pInfo->pParenthesis);
	
					pTypeName = cxxTagCheckAndSetTypeField(pInfo->pTypeStart,pInfo->pTypeEnd);
	
					cxxTokenChainInsertAfter(
							pInfo->pParenthesisContainerChain,
							pTokenBeforeParenthesis,
							pInfo->pParenthesis
						);
				} else {
					pTypeName = NULL;
				}
			} else {
				pTypeName = cxxTagCheckAndSetTypeField(pInfo->pTypeStart,pInfo->pTypeEnd);
			}
		} else {
			pTypeName = NULL;
		}

		if(pszSignature)
			tag->extensionFields.signature = vStringValue(pszSignature);

		bool bIsEmptyTemplate = false;

		if(
			g_cxx.pTemplateTokenChain && (g_cxx.pTemplateTokenChain->iCount > 0) &&
			cxxParserCurrentLanguageIsCPP() &&
			cxxTagFieldEnabled(CXXTagCPPFieldTemplate)
		)
		{
			bIsEmptyTemplate = g_cxx.pTemplateTokenChain->iCount == 2;
			cxxTokenChainNormalizeTypeNameSpacing(g_cxx.pTemplateTokenChain);
			cxxTokenChainCondense(g_cxx.pTemplateTokenChain,0);
			cxxTagSetField(
					CXXTagCPPFieldTemplate,
					vStringValue(cxxTokenChainFirst(g_cxx.pTemplateTokenChain)->pszWord)
				);
		}

		vString * pszProperties = NULL;

		if(cxxTagFieldEnabled(CXXTagFieldProperties))
		{
			unsigned int uProperties = 0;

			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenVirtual)
				uProperties |= CXXTagPropertyVirtual;
			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenStatic)
				uProperties |= CXXTagPropertyStatic;
			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenInline)
				uProperties |= CXXTagPropertyInline;
			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenExplicit)
				uProperties |= CXXTagPropertyExplicit; // FIXME: Handle "CXXTagPropertyConstructor"?
			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenExtern)
				uProperties |= CXXTagPropertyExtern;
			if(g_cxx.uKeywordState & CXXParserKeywordStateSeenAttributeDeprecated)
				uProperties |= CXXTagPropertyDeprecated;
			if(pInfo->pSignatureConst)
				uProperties |= CXXTagPropertyConst;
			if(pInfo->uFlags & CXXFunctionSignatureInfoPure)
				uProperties |= CXXTagPropertyPure | CXXTagPropertyVirtual;
			if(pInfo->uFlags & CXXFunctionSignatureInfoOverride)
				uProperties |= CXXTagPropertyOverride | CXXTagPropertyVirtual;
			if(pInfo->uFlags & CXXFunctionSignatureInfoFinal)
				uProperties |= CXXTagPropertyFinal | CXXTagPropertyVirtual;
			if(pInfo->uFlags & CXXFunctionSignatureInfoDefault)
				uProperties |= CXXTagPropertyDefault;
			if(pInfo->uFlags & CXXFunctionSignatureInfoDelete)
				uProperties |= CXXTagPropertyDelete;
			if(pInfo->uFlags & CXXFunctionSignatureInfoVolatile)
				uProperties |= CXXTagPropertyVolatile;
			if(pInfo->uFlags & CXXFunctionSignatureInfoScopeTemplateSpecialization)
				uProperties |= CXXTagPropertyScopeTemplateSpecialization |
								CXXTagPropertyTemplateSpecialization;
			if((pInfo->uFlags & CXXFunctionSignatureInfoTemplateSpecialization) || bIsEmptyTemplate)
				uProperties |= CXXTagPropertyTemplateSpecialization;

			pszProperties = cxxTagSetProperties(uProperties);
		}

		int iCorkQueueIndex = cxxTagCommit();

		if(piCorkQueueIndex)
			*piCorkQueueIndex = iCorkQueueIndex;

		if(pszSignature)
			vStringDelete(pszSignature);

		if(pszProperties)
			vStringDelete(pszProperties);

		if(pTypeName)
			cxxTokenDestroy(pTypeName);
	}


#ifdef CXX_DO_DEBUGGING
	if(uTagKind == CXXTagKindFUNCTION)
		CXX_DEBUG_PRINT("Emitted function '%s'",vStringValue(pIdentifier->pszWord));
	else
		CXX_DEBUG_PRINT("Emitted prototype '%s'",vStringValue(pIdentifier->pszWord));
#endif

	if(bPushScopes)
	{
		cxxScopePush(pIdentifier,CXXScopeTypeFunction,CXXScopeAccessUnknown);
		iScopesPushed++;
	} else {
		cxxTokenDestroy(pIdentifier);
	}

	CXX_DEBUG_LEAVE();
	return iScopesPushed;
}

//
// This is called at block level upon encountering an opening bracket,
// when we are not in a function. The current block chain almost certainly
// contains a function signature.
//
// This function attempts to extract the function name, emit it as a tag
// and push all the necessary scopes for the next block. It returns the number
// of scopes pushed.
//
int cxxParserExtractFunctionSignatureBeforeOpeningBracket(int * piCorkQueueIndex)
{
	CXX_DEBUG_ENTER();

#ifdef CXX_DO_DEBUGGING
	vString * pChain = cxxTokenChainJoin(g_cxx.pTokenChain,NULL,0);
	CXX_DEBUG_PRINT("Looking for function in '%s'",vStringValue(pChain));
	vStringDelete(pChain);
#endif

	// Note that the token chain ALWAYS contains the final delimiter here.

	CXX_DEBUG_ASSERT(
			g_cxx.pTokenChain->iCount > 0,
			"There should be at least the terminator here!"
		);
	CXX_DEBUG_ASSERT(
			cxxTokenChainLast(g_cxx.pTokenChain)->eType == CXXTokenTypeOpeningBracket,
			"We should have been called when pointing on an opening bracket!"
		);

	cxxTokenChainDestroyLast(g_cxx.pTokenChain);

	CXXFunctionSignatureInfo oInfo;

	CXXFunctionParameterInfo oParamInfo;

	if(!cxxParserLookForFunctionSignature(g_cxx.pTokenChain,&oInfo,&oParamInfo))
	{
		CXX_DEBUG_LEAVE_TEXT("No parenthesis found: no function");
		return 0;
	}

	int iScopesPushed = cxxParserEmitFunctionTags(
			&oInfo,
			CXXTagKindFUNCTION,
			CXXEmitFunctionTagsPushScopes,
			piCorkQueueIndex
		);

	if(cxxTagKindEnabled(CXXTagKindPARAMETER))
		cxxParserEmitFunctionParameterTags(&oParamInfo);

	CXX_DEBUG_LEAVE();
	return iScopesPushed;
}

// This function *may* change the token chain
void cxxParserEmitFunctionParameterTags(CXXFunctionParameterInfo * pInfo)
{
	// emit parameters

	unsigned int i = 0;
	while(i < pInfo->uParameterCount)
	{
		tagEntryInfo * tag = cxxTagBegin(
				CXXTagKindPARAMETER,
				pInfo->aIdentifiers[i]
			);

		if(!tag)
			return;

		CXXToken * pTypeName;

		if(pInfo->aDeclarationStarts[i] && pInfo->aDeclarationEnds[i])
		{
			// This is tricky.
			// We know that the declaration contains the identifier.
			// We don't want the identifier to appear in the type name.
			// So we have to remove it from the chain (eventually recursively if there
			// are nested parentheses).
			// However the declaration might start or end with the identifier
			// and in that case we would be effectively breaking the type chain.
			// Work around it.

			CXXToken * pTypeStart = pInfo->aDeclarationStarts[i];
			CXXToken * pTypeEnd = pInfo->aDeclarationEnds[i];

			if(pTypeStart != pTypeEnd)
			{
				if(pTypeStart == pInfo->aIdentifiers[i])
					pTypeStart = pTypeStart->pNext;
				else if(pTypeEnd == pInfo->aIdentifiers[i])
					pTypeEnd = pTypeEnd->pPrev;

				cxxTokenChainTakeRecursive(pInfo->pChain,pInfo->aIdentifiers[i]);

				pTypeName = cxxTagCheckAndSetTypeField(
						pTypeStart,
						pTypeEnd
					);
			} else {
				// The declaration contains only the identifier!
				pTypeName = NULL;
			}
		} else {
			pTypeName = NULL;
		}

		tag->isFileScope = true;
		cxxTagCommit();

		if(pTypeName)
		{
			cxxTokenDestroy(pInfo->aIdentifiers[i]);
			cxxTokenDestroy(pTypeName);
		}

		i++;
	}
}



//
// This function checks if the specified token chain looks like a
// non K&R style function parameter list, eventually with default arguments
// and such.
//
// If pParamInfo is non NULL then the function will also gather
// informations about the parameters and store them.
//
bool cxxParserTokenChainLooksLikeFunctionParameterList(
		CXXTokenChain * tc,
		CXXFunctionParameterInfo * pParamInfo
	)
{
	CXX_DEBUG_ENTER();
	CXX_DEBUG_ASSERT(
			tc->iCount >= 2,
			"At least linitial and final parenthesis should be there"
		);

	if(pParamInfo)
	{
		pParamInfo->uParameterCount = 0;
		pParamInfo->pChain = tc;
	}

	if(tc->iCount == 2)
	{
		CXX_DEBUG_LEAVE_TEXT("Empty signature is valid for a function");
		return true;
	}

	CXXToken * t = cxxTokenChainAt(tc,1);

	bool bIsC = cxxParserCurrentLanguageIsC();

	for(;;)
	{
		// Check every parameter.
		//
		// Possibilities:
		//
		//    type variable
		//    type /* variable omitted */
		//    type variable[..]
		//    type variable:bits
		//    type (*variable)(args)
		//    <anything of the above> = default <-- C++ only
		//    ... <-- vararg
		//

		CXXToken * pStart = t;

		// First token must be identifier/keyword or ...
		if(!cxxTokenTypeIsOneOf(
				t,
				CXXTokenTypeIdentifier | CXXTokenTypeKeyword | CXXTokenTypeMultipleDots
			))
		{
			CXX_DEBUG_LEAVE_TEXT(
					"Token '%s' is something that is not a identifier, keyword or ...",
					vStringValue(t->pszWord)
				);
			return false;
		}

#define TOKENS_THAT_SHOULD_NOT_APPEAR_IN_SIGNATURE_BEFORE_ASSIGNMENT \
		( \
			CXXTokenTypePointerOperator | \
			CXXTokenTypeOperator | \
			CXXTokenTypeDotOperator | \
			CXXTokenTypeNumber | \
			CXXTokenTypeStringConstant | \
			CXXTokenTypeCharacterConstant | \
			CXXTokenTypeAngleBracketChain | \
			CXXTokenTypeSingleColon \
		)

try_again:
		t = cxxTokenChainNextTokenOfType(
				t,
				CXXTokenTypeClosingParenthesis | CXXTokenTypeComma |
					CXXTokenTypeAssignment | CXXTokenTypeSmallerThanSign |
					CXXTokenTypeGreaterThanSign | CXXTokenTypeParenthesisChain |
				TOKENS_THAT_SHOULD_NOT_APPEAR_IN_SIGNATURE_BEFORE_ASSIGNMENT
			);

		CXX_DEBUG_ASSERT(t,"We should have found the closing parenthesis here");

		if(cxxTokenTypeIs(t,CXXTokenTypeParenthesisChain))
		{
			CXX_DEBUG_PRINT("Found parenthesis chain");
			// Either part of function pointer declaration or a very ugly variable decl
			// Examples are:
			//    type (*name)(args)
			//    type (*name)
			//    type (&name)
			//    type (&name)[something]
			//    ...
			//
			// FIXME: This check should be stricter (?)
			if(
				(
					!cxxTokenChainFirstTokenOfType(
							t->pChain,
							TOKENS_THAT_SHOULD_NOT_APPEAR_IN_SIGNATURE_BEFORE_ASSIGNMENT
						)
				) && (
					cxxTokenChainFirstTokenOfType(
							t->pChain,
							CXXTokenTypeStar | CXXTokenTypeAnd
						) || // part of (*name) or (&name)
					cxxParserTokenChainLooksLikeFunctionParameterList(
							t->pChain,
							NULL
						) // args
				)
			)
				goto try_again;

			CXX_DEBUG_LEAVE_TEXT(
					"Found a parenthesis chain that doesn't belong to a function parameters list"
				);
			return false;
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeSmallerThanSign))
		{
			CXX_DEBUG_PRINT("Maybe template?");

			t = cxxTokenChainSkipToEndOfTemplateAngleBracket(t);

			if(!t)
			{
				CXX_DEBUG_LEAVE_TEXT(
						"Either not a function declaration or unbalanced " \
							"template angle brackets"
					);
				return false;
			}

			goto try_again;
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeGreaterThanSign))
		{
			CXX_DEBUG_LEAVE_TEXT("Unbalanced > (a < should have been found before)");
			return false;
		}

		if(cxxTokenTypeIsOneOf(
				t,
				TOKENS_THAT_SHOULD_NOT_APPEAR_IN_SIGNATURE_BEFORE_ASSIGNMENT
			))
		{
			CXX_DEBUG_LEAVE_TEXT(
					"Token '%s' is something that doesn't belong to a function " \
						"parameter list",
					vStringValue(t->pszWord)
				);
			return false;
		}

		// closing parenthesis, assignment or comma

		if(pParamInfo && (t->pPrev != pStart))
		{
			// FIXME: This may break in some special macro cases?
			if(pParamInfo->uParameterCount < CXX_MAX_EXTRACTED_PARAMETERS)
			{
				// locate identifier

				CXXToken * pIdentifier = NULL;

				if(cxxTokenTypeIs(t->pPrev,CXXTokenTypeIdentifier))
				{
					// type var
					pIdentifier = t->pPrev;
				} else if(t->pPrev->pPrev)
				{
					bool bPrevIsSquareParenthesis = cxxTokenTypeIs(
							t->pPrev,
							CXXTokenTypeSquareParenthesisChain
						);

					if(
						bPrevIsSquareParenthesis &&
						cxxTokenTypeIs(t->pPrev->pPrev,CXXTokenTypeIdentifier)
					)
					{
						// type var[]
						pIdentifier = t->pPrev->pPrev;
					} else if(
						bPrevIsSquareParenthesis &&
						cxxTokenTypeIs(t->pPrev->pPrev,CXXTokenTypeParenthesisChain) &&
						(pIdentifier = cxxTokenChainFirstTokenOfType(
								t->pPrev->pPrev->pChain,
								CXXTokenTypeIdentifier
							))
					)
					{
						// type (...var)[]
					} else if(
						cxxTokenTypeIs(t->pPrev,CXXTokenTypeNumber) &&
						cxxTokenTypeIs(t->pPrev->pPrev,CXXTokenTypeIdentifier)
					)
					{
						// type var:bits
						pIdentifier = t->pPrev->pPrev;
					} else if(
						cxxTokenTypeIs(t->pPrev,CXXTokenTypeParenthesisChain) &&
						(
							(
								// type (*name)(args)
								cxxTokenTypeIs(
										t->pPrev->pPrev,
										CXXTokenTypeParenthesisChain
									) &&
								(pIdentifier = cxxTokenChainLastPossiblyNestedTokenOfType(
										t->pPrev->pPrev->pChain,
										CXXTokenTypeIdentifier
									)) &&
								pIdentifier->pPrev &&
								cxxTokenTypeIs(pIdentifier->pPrev,CXXTokenTypeStar)
							) || (
								// type (*&name)
								(pIdentifier = cxxTokenChainLastPossiblyNestedTokenOfType(
										t->pPrev->pChain,
										CXXTokenTypeIdentifier
									)) &&
								pIdentifier->pPrev &&
								cxxTokenTypeIsOneOf(
										pIdentifier->pPrev,
										CXXTokenTypeStar | CXXTokenTypeAnd
									)
							)
						)
					)
					{
						// type (*ptr)(args)
						// pIdentifier already set above
						// FIXME: Check this better?
					}
				}

				if(pIdentifier)
				{
					pParamInfo->aDeclarationStarts[pParamInfo->uParameterCount] = pStart;
					pParamInfo->aDeclarationEnds[pParamInfo->uParameterCount] = t->pPrev;
					pParamInfo->aIdentifiers[pParamInfo->uParameterCount] = pIdentifier;
					pParamInfo->uParameterCount++;

#ifdef CXX_DO_DEBUGGING
					CXXToken * pDecl = cxxTokenChainExtractRange(pStart,t->pPrev,0);
					CXX_DEBUG_PRINT(
							"Found parameter '%s' in '%s'",
							vStringValue(pIdentifier->pszWord),
							vStringValue(pDecl->pszWord)
						);
					cxxTokenDestroy(pDecl);
					CXX_DEBUG_ASSERT(
							cxxTokenChainFindToken(pParamInfo->pChain,pStart) >= 0,
							"The start token must be in the chain"
						);
					CXX_DEBUG_ASSERT(
							cxxTokenChainFindToken(pParamInfo->pChain,t->pPrev) >= 0,
							"The end token must be in the chain"
						);
#endif
				}
			} else {
				pParamInfo = NULL; // reset so condition will be faster to check
			}
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeClosingParenthesis))
		{
			CXX_DEBUG_LEAVE_TEXT("Found closing parenthesis, it's OK");
			return true;
		}

		if(cxxTokenTypeIs(t,CXXTokenTypeComma))
		{
			// ok, go ahead
			CXX_DEBUG_PRINT("Found comma");
			t = t->pNext;
			continue;
		}

		// assignment.
		if(bIsC)
		{
			CXX_DEBUG_LEAVE_TEXT(
					"Found assignment, this doesn't look like valid C function parameter list"
				);
			return false;
		}

		CXX_DEBUG_PRINT("Found assignment");

		t = cxxTokenChainNextTokenOfType(t,CXXTokenTypeClosingParenthesis | CXXTokenTypeComma);

		CXX_DEBUG_ASSERT(t,"We should have found the closing parenthesis here");

		if(cxxTokenTypeIs(t,CXXTokenTypeClosingParenthesis))
		{
			CXX_DEBUG_LEAVE_TEXT("Found closing parenthesis, it's OK");
			return true;
		}

		// ok, comma
		t = t->pNext;
	}

	// not reached
	CXX_DEBUG_LEAVE();
	return true;
}
