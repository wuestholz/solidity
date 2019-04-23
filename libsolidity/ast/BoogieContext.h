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
		boogie::Expr::Ref expr; // Expression converted to Boogie
		std::string exprStr; // Expression in original format
		std::list<boogie::Expr::Ref> tccs; // TCCs for the expression
		std::list<boogie::Expr::Ref> ocs; // OCs for the expression

		DocTagExpr(boogie::Expr::Ref expr, std::string exprStr,
				std::list<boogie::Expr::Ref> const& tccs,
				std::list<boogie::Expr::Ref> const& ocs) :
			expr(expr), exprStr(exprStr), tccs(tccs), ocs(ocs) {}
	};

private:

	boogie::Program m_program; // Result of the conversion is a single Boogie program (top-level node)

	Encoding m_encoding;
	bool m_overflow;
	langutil::ErrorReporter* m_errorReporter; // Report errors with this member
	langutil::Scanner const* m_currentScanner; // Scanner used to resolve locations in the original source

	// Some members required to parse invariants. (Invariants can be found
	// in comments, so they are not parsed when the contract is parsed.)
	std::vector<Declaration const*> m_globalDecls;
	std::vector<FunctionType*> m_verifierSumTypes;
	std::vector<MagicVariableDeclaration*> m_verifierSum;
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> m_scopes;
	langutil::EVMVersion m_evmVersion;

	std::list<DocTagExpr> m_currentContractInvars; // Invariants for the current contract (in Boogie and original format)
	std::map<Declaration const*, TypePointer> m_currentSumDecls; // List of declarations that need shadow variable to sum

	typedef std::map<std::string, boogie::Decl::ConstRef> builtin_cache;
	builtin_cache m_builtinFunctions;

	void addBuiltinFunction(boogie::FuncDeclRef fnDecl);

	bool m_transferIncluded;
	bool m_callIncluded;
	bool m_sendIncluded;

public:

	BoogieContext(Encoding encoding,
			bool overflow,
			langutil::ErrorReporter* errorReporter,
			std::vector<Declaration const*> globalDecls,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes,
			langutil::EVMVersion evmVersion);

	~BoogieContext();

	boogie::Program& program() { return m_program; }
	Encoding encoding() const { return m_encoding; }
	bool isBvEncoding() const { return m_encoding == Encoding::BV; }
	bool overflow() const { return m_overflow; }
	langutil::ErrorReporter*& errorReporter() { return m_errorReporter; }
	langutil::Scanner const*& currentScanner() { return m_currentScanner; }
	std::vector<Declaration const*>& globalDecls() { return m_globalDecls; }
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>>& scopes() { return m_scopes; }
	langutil::EVMVersion& evmVersion() { return m_evmVersion; }
	std::list<DocTagExpr>& currentContractInvars() { return m_currentContractInvars; }
	std::map<Declaration const*, TypePointer>& currentSumDecls() { return m_currentSumDecls; }

	void includeTransferFunction();
	void includeCallFunction();
	void includeSendFunction();

	void reportError(ASTNode const* associatedNode, std::string message);
	void reportWarning(ASTNode const* associatedNode, std::string message);

	/** Returns the integer type corresponding to the encoding */
	std::string intType(unsigned size) const;

	/** Slice of an integer corresponding to the encoding */
	boogie::Expr::Ref intSlice(boogie::Expr::Ref base, unsigned size, unsigned high, unsigned low);

	// Parametrized BV operations
	boogie::Expr::Ref bvExtract(boogie::Expr::Ref expr, unsigned size, unsigned high, unsigned low);
	boogie::Expr::Ref bvZeroExt(boogie::Expr::Ref expr, unsigned exprSize, unsigned resultSize);
	boogie::Expr::Ref bvSignExt(boogie::Expr::Ref expr, unsigned exprSize, unsigned resultSize);

	// Binary BV operations
	boogie::Expr::Ref bvAdd(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSub(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvMul(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSDiv(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvUDiv(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvAnd(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvOr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvXor(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvAShr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvLShr(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvShl(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSlt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvUlt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSgt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvUgt(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSle(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvUle(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvSge(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);
	boogie::Expr::Ref bvUge(unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);

	// Unary BV operation
	boogie::Expr::Ref bvNeg(unsigned bits, boogie::Expr::Ref expr);
	boogie::Expr::Ref bvNot(unsigned bits, boogie::Expr::Ref expr);

	// Low lever interface
	boogie::Expr::Ref bvBinaryOp(std::string name, unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, std::string resultType = "");
	boogie::Expr::Ref bvUnaryOp(std::string name, unsigned bits, boogie::Expr::Ref expr);

};

}
}
