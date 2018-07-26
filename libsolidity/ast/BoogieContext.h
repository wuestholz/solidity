#pragma once

#include <libsolidity/interface/ErrorReporter.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/DeclarationContainer.h>
#include <libsolidity/interface/EVMVersion.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/parsing/Scanner.h>

namespace dev
{
namespace solidity
{

class BoogieContext {
	bool m_bitPrecise;
	ErrorReporter& m_errorReporter;
	Scanner const* m_currentScanner;
	// Some members required to parse invariants. (Invariants can be found
	// in comments, so they are not parsed when the contract is parsed.)
	std::vector<Declaration const*> m_globalDecls;
	MagicVariableDeclaration m_verifierSum;
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> m_scopes;
	EVMVersion m_evmVersion;

	std::map<smack::Expr const*, std::string> m_currentInvars; // Invariants for the current contract (in Boogie and original format)
	std::list<Declaration const*> m_currentSumDecls; // List of declarations that need shadow variable to sum
	std::map<std::string, smack::FuncDecl const*> m_bvBuiltinFunctions;

public:
	BoogieContext(bool bitPrecise, ErrorReporter& errorReporter, std::vector<Declaration const*> globalDecls,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes, EVMVersion evmVersion)
		: m_bitPrecise(bitPrecise), m_errorReporter(errorReporter), m_currentScanner(nullptr), m_globalDecls(globalDecls),
		  m_verifierSum(ASTBoogieUtils::VERIFIER_SUM, std::make_shared<FunctionType>(strings{}, strings{"int"}, FunctionType::Kind::Internal, true, StateMutability::Pure)),
		  m_scopes(scopes), m_evmVersion(evmVersion), m_currentInvars(), m_currentSumDecls(), m_bvBuiltinFunctions()
	{
		m_globalDecls.push_back(&m_verifierSum);
	}

	bool bitPrecise() { return m_bitPrecise; }
	ErrorReporter& errorReporter() { return m_errorReporter; }
	Scanner const*& currentScanner() { return m_currentScanner; }
	std::vector<Declaration const*>& globalDecls() { return m_globalDecls; }
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>>& scopes() { return m_scopes; }
	EVMVersion& evmVersion() { return m_evmVersion; }
	std::map<smack::Expr const*, std::string>& currentInvars() { return m_currentInvars; }
	std::list<Declaration const*>& currentSumDecls() { return m_currentSumDecls; }
	std::map<std::string, smack::FuncDecl const*>& bvBuiltinFunctions() { return m_bvBuiltinFunctions; }

};

}
}
