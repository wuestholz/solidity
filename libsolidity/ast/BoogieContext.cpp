#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/BoogieContext.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{
BoogieContext::BoogieContext(Encoding encoding, ErrorReporter* errorReporter, std::vector<Declaration const*> globalDecls,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes, EVMVersion evmVersion) :
					m_program(),
					m_encoding(encoding),
					m_errorReporter(errorReporter),
					m_currentScanner(nullptr),
					m_globalDecls(globalDecls),

					m_scopes(scopes),
				  	m_evmVersion(evmVersion),
				  	m_currentContractInvars(),
				  	m_currentSumDecls(),
				  	m_builtinFunctions(),
				  	m_transferIncluded(false),
				  	m_callIncluded(false),
				  	m_sendIncluded(false)
{
	// Add magic variables for the sum function for all sizes of int and uint
	for (string sign : {"", "u"})
	{
		for (int i = 8; i <= 256; i += 8)
		{
			string type = sign + "int" + to_string(i);
			// TODO: should be deleted in destructor of context
			auto sum = new MagicVariableDeclaration(ASTBoogieUtils::VERIFIER_SUM + "_" + type,
									std::make_shared<FunctionType>(strings{}, strings{type},
											FunctionType::Kind::Internal, true, StateMutability::Pure));
			m_verifierSum.push_back(sum);
			m_globalDecls.push_back(sum);
		}
	}

}

void BoogieContext::includeBuiltInFunction(std::string name, smack::FuncDecl* decl)
{
	if (m_builtinFunctions.find(name) != m_builtinFunctions.end()) return;
	m_builtinFunctions.insert(name);
	m_program.getDeclarations().push_back(decl);
}

void BoogieContext::includeTransferFunction()
{
	if (m_transferIncluded) return;
	m_transferIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createTransferProc(*this));
}

void BoogieContext::includeCallFunction()
{
	if (m_callIncluded) return;
	m_callIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createCallProc(*this));
}

void BoogieContext::includeSendFunction()
{
	if (m_sendIncluded) return;
	m_sendIncluded = true;
	m_program.getDeclarations().push_back(ASTBoogieUtils::createSendProc(*this));
}

void BoogieContext::reportError(ASTNode const* associatedNode, std::string message)
{
	if (associatedNode)
	{
		m_errorReporter->error(Error::Type::ParserError, associatedNode->location(), message);
	}
	else
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Error at unknown node: " + message));
	}
}

void BoogieContext::reportWarning(ASTNode const* associatedNode, std::string message)
{
	if (associatedNode)
	{
		m_errorReporter->warning(associatedNode->location(), message);
	}
	else
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Warning at unknown node: " + message));
	}
}


}
}
