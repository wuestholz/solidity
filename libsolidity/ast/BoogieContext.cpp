#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/BoogieContext.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{
BoogieContext::BoogieContext(bool bitPrecise, ErrorReporter& errorReporter, std::vector<Declaration const*> globalDecls,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes, EVMVersion evmVersion) :
					m_program(),
					m_bitPrecise(bitPrecise),
					m_errorReporter(errorReporter),
					m_currentScanner(nullptr),
					m_globalDecls(globalDecls),
					m_verifierSum(ASTBoogieUtils::VERIFIER_SUM, std::make_shared<FunctionType>(strings{}, strings{"int"}, FunctionType::Kind::Internal, true, StateMutability::Pure)),
					m_scopes(scopes),
				  	m_evmVersion(evmVersion),
				  	m_currentInvars(),
				  	m_currentSumDecls(),
				  	m_builtinFunctions(),
				  	m_transferIncluded(false),
				  	m_callIncluded(false),
				  	m_sendIncluded(false)
{
	m_globalDecls.push_back(&m_verifierSum);
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


}
}
