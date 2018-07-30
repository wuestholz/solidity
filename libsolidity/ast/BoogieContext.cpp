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
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes, EVMVersion evmVersion)
		: m_bitPrecise(bitPrecise), m_errorReporter(errorReporter), m_currentScanner(nullptr), m_globalDecls(globalDecls),
		  m_verifierSum(ASTBoogieUtils::VERIFIER_SUM, std::make_shared<FunctionType>(strings{}, strings{"int"}, FunctionType::Kind::Internal, true, StateMutability::Pure)),
		  m_scopes(scopes), m_evmVersion(evmVersion), m_currentInvars(), m_currentSumDecls(), m_bvBuiltinFunctions()
	{
		m_globalDecls.push_back(&m_verifierSum);
	}
}
}
