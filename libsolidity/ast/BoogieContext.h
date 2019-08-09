#pragma once

#include <liblangutil/ErrorReporter.h>
#include <liblangutil/EVMVersion.h>
#include <liblangutil/Scanner.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/GlobalContext.h>
#include <libsolidity/analysis/DeclarationContainer.h>
#include <libsolidity/ast/ASTBoogieStats.h>
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

	/** Encoding for arithmetic types and operations. */
	enum Encoding
	{
		INT, // Use integers
		BV,  // Use bitvectors
		MOD  // Use integers with modulo operations
	};

	/** Expression (annotation) parsed from a documentation tag. */
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
	/** Path in an array/mapping member that needs sum */
	struct SumPath {
		std::string base; // Base array over which we sum
		std::vector<std::shared_ptr<boogie::DtSelExpr const>> members; // Path of member accesses
	};

	/** Specification for an array/mapping that needs sum */
	struct SumSpec {
		SumPath path; // Path of the member we sum over
		TypePointer type; // Type of the resulting sum
		boogie::VarDeclRef shadowVar; // Shadow variable that needs to be updated
	};

	ASTBoogieStats m_stats;
	boogie::Program m_program; // Result of the conversion is a single Boogie program (top-level node)

	std::map<std::string, boogie::Decl::Ref> m_stringLiterals;
	std::map<std::string, boogie::Decl::Ref> m_addressLiterals;

	boogie::VarDeclRef m_boogieBalance;
	boogie::VarDeclRef m_boogieThis;
	boogie::VarDeclRef m_boogieMsgSender;
	boogie::VarDeclRef m_boogieMsgValue;

	std::map<StructDefinition const*,boogie::TypeDeclRef> m_memStructTypes;
	std::map<StructDefinition const*,boogie::TypeDeclRef> m_storStructTypes;
	std::map<StructDefinition const*,boogie::FuncDeclRef> m_storStructConstrs;

	std::map<std::string,boogie::DataTypeDeclRef> m_arrDataTypes;
	std::map<std::string,boogie::FuncDeclRef> m_arrConstrs;
	std::map<std::string,boogie::TypeDeclRef> m_memArrPtrTypes;
	std::map<std::string,boogie::VarDeclRef> m_memArrs;

	std::map<std::string,boogie::FuncDeclRef> m_defaultArrays;

	Encoding m_encoding;
	bool m_overflow;
	bool m_modAnalysis;
	langutil::ErrorReporter* m_errorReporter; // Report errors with this member
	langutil::Scanner const* m_currentScanner; // Scanner used to resolve locations in the original source

	// Some members required to parse expressions from comments
	BoogieGlobalContext m_globalContext;
	std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> m_scopes;
	langutil::EVMVersion m_evmVersion;

	ContractDefinition const* m_currentContract;
	std::list<DocTagExpr> m_currentContractInvars;
	std::map<ContractDefinition const*, std::list<SumSpec>> m_currentSumSpecs;

	typedef std::map<std::string, boogie::Decl::ConstRef> builtin_cache;
	builtin_cache m_builtinFunctions;

	void addBuiltinFunction(boogie::FuncDeclRef fnDecl);

	bool m_transferIncluded;
	bool m_callIncluded;
	bool m_sendIncluded;

	// A stack of extra scopes (added to declaration names) used in inlining
	std::list<std::pair<ASTNode const*, std::string>> m_extraScopes;

	int m_nextId = 0;

public:

	BoogieContext(Encoding encoding,
			bool overflow,
			bool modAnalysis,
			langutil::ErrorReporter* errorReporter,
			std::map<ASTNode const*, std::shared_ptr<DeclarationContainer>> scopes,
			langutil::EVMVersion evmVersion,
			ASTBoogieStats stats);

	ASTBoogieStats& stats() { return m_stats; }
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
	int nextId() { return m_nextId++; }
	boogie::VarDeclRef freshTempVar(boogie::TypeDeclRef type, std::string prefix = "tmp");
	ContractDefinition const* currentContract() const { return m_currentContract; }
	void setCurrentContract(ContractDefinition const* contract) { m_currentContract = contract; }
	void printErrors(std::ostream& out);

public:
	/** Prints the Boogie program to an output stream. */
	void print(std::ostream& _stream) { m_program.print(_stream); }

	// Built-in functions and members
	void includeTransferFunction();
	void includeCallFunction();
	void includeSendFunction();
	boogie::VarDeclRef boogieBalance() const { return m_boogieBalance; }
	boogie::VarDeclRef boogieThis() const { return m_boogieThis; }
	boogie::VarDeclRef boogieMsgSender() const { return m_boogieMsgSender; }
	boogie::VarDeclRef boogieMsgValue() const { return m_boogieMsgValue; }

	/** Maps a declaration name to a name in Boogie, including extra scoping if needed. */
	std::string mapDeclName(Declaration const& decl);

	// Sum function related
private:
	/**
	 * Converts an expression to a path for summing. E.g., ss[i].x.y becomes
	 * {base: ss, members: {x, y}}
	 * Optionally a node can be given to report errors.
	 */
	void getPath(boogie::Expr::Ref expr, SumPath& path, ASTNode const* errors = nullptr);
	bool pathsEqual(SumPath const& p1, SumPath const& p2);
public:
	/** Gets the sum shadow variable for a given expression. If it does not exist, it is created. */
	boogie::Expr::Ref getSumVar(boogie::Expr::Ref bgExpr, Expression const* expr, TypePointer type);
	/** Initializes all sum shadow variables related to a given base declaration. */
	std::list<boogie::Stmt::Ref> initSumVars(Declaration const* decl);
	/** Updates sum variables of an assignment (lhs, rhs) if the lhs requires sum. */
	std::list<boogie::Stmt::Ref> updateSumVars(boogie::Expr::Ref lhsBg, boogie::Expr::Ref rhsBg);

	/** Push an extra scope for declarations under the scode of a given node. */
	void pushExtraScope(ASTNode const* node, std::string id) { m_extraScopes.push_back(std::make_pair(node, id)); }
	/** Pop an extra scope. */
	void popExtraScope() { m_extraScopes.pop_back(); }

	// Error reporting
	void reportError(ASTNode const* associatedNode, std::string message);
	void reportWarning(ASTNode const* associatedNode, std::string message);

	void addGlobalComment(std::string str);
	void addDecl(boogie::Decl::Ref decl);

	// Types

	/** Maps a Solidity type to a Boogie type. */
	boogie::TypeDeclRef toBoogieType(TypePointer tp, ASTNode const* _associatedNode);
	boogie::TypeDeclRef addressType() const;
	boogie::TypeDeclRef boolType() const;
	boogie::TypeDeclRef stringType() const;
	boogie::TypeDeclRef intType(unsigned size) const;
	boogie::TypeDeclRef localPtrType();
	boogie::TypeDeclRef errType() const { return boogie::Decl::elementarytype("__ERROR_UNSUPPORTED_TYPE"); }

	boogie::FuncDeclRef getStructConstructor(StructDefinition const* structDef);
	boogie::TypeDeclRef getStructType(StructDefinition const* structDef, DataLocation loc);

	boogie::FuncDeclRef getArrayConstructor(boogie::TypeDeclRef type) { return m_arrConstrs[type->getName()]; }
	boogie::Expr::Ref getMemArray(boogie::Expr::Ref arrPtrExpr, boogie::TypeDeclRef type) { return boogie::Expr::arrsel(m_memArrs[type->getName()]->getRefTo(), arrPtrExpr); }
	boogie::Expr::Ref getArrayLength(boogie::Expr::Ref arrayExpr, boogie::TypeDeclRef type) { return boogie::Expr::dtsel(arrayExpr, "length", m_arrConstrs[type->getName()], m_arrDataTypes[type->getName()]); }
	boogie::Expr::Ref getInnerArray(boogie::Expr::Ref arrayExpr, boogie::TypeDeclRef type) { return boogie::Expr::dtsel(arrayExpr, "arr", m_arrConstrs[type->getName()], m_arrDataTypes[type->getName()]); }

	boogie::FuncDeclRef defaultArray(boogie::TypeDeclRef keyType, boogie::TypeDeclRef valueType, std::string valueSmt);

	/** Creates an int literal corresponding to the encoding. */
	boogie::Expr::Ref intLit(boogie::bigint lit, int bits) const;
	/** Gets the Boogie representation of a string literal. */
	boogie::Expr::Ref getStringLiteral(std::string str);
	/** Gets the Boogie representation of an address literal. */
	boogie::Expr::Ref getAddressLiteral(std::string addr);
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
