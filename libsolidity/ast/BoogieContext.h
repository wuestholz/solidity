#pragma once

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/EVMVersion.h>
#include <liblangutil/Scanner.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/DeclarationContainer.h>
#include <libsolidity/ast/BoogieAst.h>
#include <set>

namespace dev
{
namespace solidity
{

/**
 * Context class that is used to pass information around the different
 * transformation classes.
 */
class BoogieContext {
public:
	/**
	 * Encoding for arithmetic types and operations.
	 */
	enum Encoding
	{
		INT, // Use integers
		BV,  // Use bitvectors
		MOD  // Use integers with modulo operations
	};

	struct DocTagExpr {
		smack::Expr const* expr; // Expression converted to Boogie
		std::string exprStr; // Expression in original format
		std::list<smack::Expr const*> tccs; // TCCs for the expression
		std::list<smack::Expr const*> ocs; // OCs for the expression

		DocTagExpr(smack::Expr const* expr, std::string exprStr, std::list<smack::Expr const*> tccs,
				std::list<smack::Expr const*> ocs) :
			expr(expr), exprStr(exprStr), tccs(tccs), ocs(ocs) {}
	};

private:
	smack::Program m_program; // Result of the conversion is a single Boogie program (top-level node)

	Encoding m_encoding;
	bool m_overflow;
	langutil::ErrorReporter* m_errorReporter; // Report errors with this member
	langutil::Scanner const* m_currentScanner; // Scanner used to resolve locations in the original source

	// Some members required to parse invariants. (Invariants can be found
	// in comments, so they are not parsed when the contract is parsed.)
	std::vector<Declaration const*> m_globalDecls;
	std::vector<MagicVariableDeclaration*> m_verifierSum;
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> m_scopes;
	langutil::EVMVersion m_evmVersion;

	std::list<DocTagExpr> m_currentContractInvars; // Invariants for the current contract (in Boogie and original format)
	std::map<Declaration const*, TypePointer> m_currentSumDecls; // List of declarations that need shadow variable to sum

	std::set<std::string> m_builtinFunctions;
	bool m_transferIncluded;
	bool m_callIncluded;
	bool m_sendIncluded;

public:
	BoogieContext(Encoding encoding, bool overflow, langutil::ErrorReporter* errorReporter, std::vector<Declaration const*> globalDecls,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes, langutil::EVMVersion evmVersion);

	smack::Program& program() { return m_program; }
	Encoding encoding() { return m_encoding; }
	bool isBvEncoding() { return m_encoding == Encoding::BV; }
	bool overflow() { return m_overflow; }
	langutil::ErrorReporter*& errorReporter() { return m_errorReporter; }
	langutil::Scanner const*& currentScanner() { return m_currentScanner; }
	std::vector<Declaration const*>& globalDecls() { return m_globalDecls; }
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>>& scopes() { return m_scopes; }
	langutil::EVMVersion& evmVersion() { return m_evmVersion; }
	std::list<DocTagExpr>& currentContractInvars() { return m_currentContractInvars; }
	std::map<Declaration const*, TypePointer>& currentSumDecls() { return m_currentSumDecls; }

	void includeBuiltInFunction(std::string name, smack::FuncDecl* decl);
	void includeTransferFunction();
	void includeCallFunction();
	void includeSendFunction();

	void reportError(ASTNode const* associatedNode, std::string message);
	void reportWarning(ASTNode const* associatedNode, std::string message);
};

}
}
