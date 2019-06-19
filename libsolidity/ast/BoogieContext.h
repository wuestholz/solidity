#pragma once

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/EVMVersion.h>
#include <liblangutil/Scanner.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/GlobalContext.h>
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
		ASTPointer<Expression> exprSol; // Expression in Solidity AST format
		std::list<boogie::Expr::Ref> tccs; // TCCs for the expression
		std::list<boogie::Expr::Ref> ocs; // OCs for the expression

		DocTagExpr() {}

		DocTagExpr(boogie::Expr::Ref expr, std::string exprStr,
				ASTPointer<Expression> exprSol,
				std::list<boogie::Expr::Ref> const& tccs,
				std::list<boogie::Expr::Ref> const& ocs) :
			expr(expr), exprStr(exprStr), exprSol(exprSol), tccs(tccs), ocs(ocs) {}
	};

	/**
	 * Global context with magic variables for verification-specific functions such as sum. We
	 * use this in the name resolver, so all other stuff is already in the scope of the resolver.
	 */
	class BoogieGlobalContext : public GlobalContext
	{
	public:
		BoogieGlobalContext();
	};

private:

	boogie::Program m_program; // Result of the conversion is a single Boogie program (top-level node)
	std::list<boogie::Decl::Ref> m_constants; // Constants declared (e.g., address/string literals)
	boogie::Decl::Ref m_boogieBalance;
	boogie::Decl::Ref m_boogieThis;
	std::map<StructDefinition const*,boogie::TypeDeclRef> m_memStructTypes;
	std::map<StructDefinition const*,boogie::TypeDeclRef> m_storStructTypes;
	std::map<StructDefinition const*,boogie::FuncDeclRef> m_storStructConstrs;

	Encoding m_encoding;
	bool m_overflow;
	bool m_modAnalysis;
	langutil::ErrorReporter* m_errorReporter; // Report errors with this member
	langutil::Scanner const* m_currentScanner; // Scanner used to resolve locations in the original source

	// Some members required to parse expressions from comments
	BoogieGlobalContext m_globalContext;
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

	std::list<std::pair<ASTNode const*, std::string>> m_extraScopes;

public:

	BoogieContext(Encoding encoding,
			bool overflow,
			bool modAnalysis,
			langutil::ErrorReporter* errorReporter,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes,
			langutil::EVMVersion evmVersion);

	Encoding encoding() const { return m_encoding; }
	bool isBvEncoding() const { return m_encoding == Encoding::BV; }
	bool overflow() const { return m_overflow; }
	bool modAnalysis() const { return m_modAnalysis; }
	langutil::ErrorReporter*& errorReporter() { return m_errorReporter; }
	langutil::Scanner const*& currentScanner() { return m_currentScanner; }
	GlobalContext* globalContext() { return &m_globalContext; }
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>>& scopes() { return m_scopes; }
	langutil::EVMVersion& evmVersion() { return m_evmVersion; }
	std::list<DocTagExpr>& currentContractInvars() { return m_currentContractInvars; }
	std::map<Declaration const*, TypePointer>& currentSumDecls() { return m_currentSumDecls; }

	/**
	 * Map a declaration name to a name in Boogie
	 */
	std::string mapDeclName(Declaration const& decl);

	/**
	 * Map a structure member with a given data location to a name in Boogie
	 */
	std::string mapStructMemberName(Declaration const& decl, DataLocation loc);

	/**
	 * Print the actual Boogie program to an output stream
	 */
	void print(std::ostream& _stream) { m_program.print(_stream); }

	void includeTransferFunction();
	void includeCallFunction();
	void includeSendFunction();

	void pushExtraScope(ASTNode const* node, std::string id) { m_extraScopes.push_back(std::make_pair(node, id)); }
	void popExtraScope() { m_extraScopes.pop_back(); }

	void reportError(ASTNode const* associatedNode, std::string message);
	void reportWarning(ASTNode const* associatedNode, std::string message);

	void addGlobalComment(std::string str);
	void addDecl(boogie::Decl::Ref decl);
	void addConstant(boogie::Decl::Ref decl);

	boogie::TypeDeclRef addressType() const;
	boogie::TypeDeclRef boolType() const;
	boogie::TypeDeclRef stringType() const;
	/** Returns the integer type corresponding to the encoding */
	boogie::TypeDeclRef intType(unsigned size) const;

	boogie::FuncDeclRef createStructConstructor(StructDefinition const* structDef);
	boogie::TypeDeclRef getStructType(StructDefinition const* structDef, DataLocation loc);

	boogie::Expr::Ref boogieBalance() const;
	boogie::Expr::Ref boogieThis() const;
	boogie::Expr::Ref boogieMsgSender() const;
	boogie::Expr::Ref boogieMsgValue() const;

	boogie::Expr::Ref intLit(boogie::bigint lit, int bits) const;

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
	boogie::Expr::Ref bvBinaryOp(std::string name, unsigned bits, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, boogie::TypeDeclRef resultType = nullptr);
	boogie::Expr::Ref bvUnaryOp(std::string name, unsigned bits, boogie::Expr::Ref expr);

};

}
}
