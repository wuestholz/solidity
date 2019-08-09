#pragma once

#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/BoogieContext.h>
#include <liblangutil/Scanner.h>
#include <string>

namespace dev
{
namespace solidity
{

/**
 * Utility class for the Solidity -> Boogie conversion.
 */
class ASTBoogieUtils
{
public:

	// Identifiers related to the 'address' type
	static std::string const SOLIDITY_BALANCE;
	static std::string const SOLIDITY_TRANSFER;
	static std::string const BOOGIE_TRANSFER;
	static std::string const SOLIDITY_SEND;
	static std::string const BOOGIE_SEND;
	static std::string const SOLIDITY_CALL;
	static std::string const BOOGIE_CALL;

	// Contract related identifiers
	static std::string const SOLIDITY_SUPER;
	static std::string const SOLIDITY_THIS;

	// Identifiers related to 'msg'
	static std::string const SOLIDITY_SENDER;
	static std::string const SOLIDITY_VALUE;

	// Error handling
	static std::string const SOLIDITY_ASSERT;
	static std::string const SOLIDITY_REQUIRE;
	static std::string const SOLIDITY_REVERT;

	// Other identifiers
	static std::string const VERIFIER_SUM;
	static std::string const VERIFIER_IDX;
	static std::string const VERIFIER_OLD;
	static std::string const VERIFIER_EQ;
	static std::string const BOOGIE_CONSTRUCTOR;
	static std::string const BOOGIE_ZERO_ADDRESS;
	static std::string const SOLIDITY_NOW;
	static std::string const BOOGIE_NOW;
	static std::string const SOLIDITY_NUMBER;
	static std::string const BOOGIE_BLOCKNO;
	static std::string const VERIFIER_OVERFLOW;

	// Return this expression as an identifier when something cannot be evaluated
	static std::string const ERR_EXPR;

	// Specification annotations
	static std::string const DOCTAG_CONTRACT_INVAR;
	static std::string const DOCTAG_LOOP_INVAR;
	static std::string const DOCTAG_CONTRACT_INVARS_INCLUDE;
	static std::string const DOCTAG_PRECOND;
	static std::string const DOCTAG_POSTCOND;
	static std::string const DOCTAG_MODIFIES;
	static std::string const DOCTAG_MODIFIES_ALL;
	static std::string const DOCTAG_MODIFIES_COND;

	/** Creates the procedure corresponding to address.transfer(). */
	static
	boogie::ProcDeclRef createTransferProc(BoogieContext& context);
	/** Creates the procedure corresponding to address.call(). */
	static
	boogie::ProcDeclRef createCallProc(BoogieContext& context);
	/** Creates the procedure corresponding to address.send().*/
	static
	boogie::ProcDeclRef createSendProc(BoogieContext& context);

	/** Data locations as strings. */
	static
	std::string dataLocToStr(DataLocation loc);

	/** Gets the Boogie constructor name. */
	static
	std::string getConstructorName(ContractDefinition const* contract);

	/** Creates attributes for original source location with message. */
	static
	std::vector<boogie::Attr::Ref> createAttrs(langutil::SourceLocation const& loc, std::string const& message, langutil::Scanner const& scanner);

	/** An expression with the associated correctness condition. */
	struct ExprWithCC {
		boogie::Expr::Ref expr;
		boogie::Expr::Ref cc;
	};

	/**
	 * Convert an arithmetic operation (including ops like +=) to an expression (with CC).
	 * The associatedNode is only used for error reporting.
	 */
	static
	ExprWithCC encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, unsigned bits, bool isSigned);

	/**
	 * Convert an arithmetic operation to an expression (with CC).
	 * The associatedNode is only used for error reporting.
	 */
	static
	ExprWithCC encodeArithUnaryOp(BoogieContext& context, ASTNode const* associatedNode, langutil::Token op, boogie::Expr::Ref subExpr, unsigned bits, bool isSigned);

	/** Checks if a type is represented with bitvectors. */
	static bool isBitPreciseType(TypePointer type);
	/** Gets the number of bits required to represent type. */
	static unsigned getBits(TypePointer type);
	/** Checks if the type is signed. */
	static bool isSigned(TypePointer type);

	/**
	 * Check if implicit conversion is needed from exprType
	 * to targetType. If yes, the conversion expression is returned, otherwise expr is
	 * returned unchanged. For example, if exprType is uint32 and targetType is uint40,
	 * then we return an extension of 8 bits to expr.
	 */
	static
	boogie::Expr::Ref checkImplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);

	/** Checks if explicit conversions are possible. */
	static
	boogie::Expr::Ref checkExplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);

	/**
	 * Get the type checking condition for an expression with a given type.
	 * Depending on the context, the returned TCC can be assumed or asserted.
	 */
	static
	boogie::Expr::Ref getTCCforExpr(boogie::Expr::Ref expr, TypePointer tp);

	/** Helper method to give a default value for a type. */
	static
	boogie::Expr::Ref defaultValue(TypePointer _type, BoogieContext& context);

private:
	/** Helper structure for getting default value of a type, including
	 * complex types such as arrays and structs.
	 */
	struct DefVal {
		std::string smt; // Default value as SMT expression (needed for arrays)
		boogie::Expr::Ref bgExpr; // Default value as Boogie expression
	};
	static
	DefVal defaultValueInternal(TypePointer _type, BoogieContext& context);

public:
	/** Allocates a new memory struct. */
	static
	boogie::Decl::Ref newStruct(StructDefinition const* structDef, BoogieContext& context);

	/** Allocates a new memory array. */
	static
	boogie::Decl::Ref newArray(boogie::TypeDeclRef type, BoogieContext& context);

	// Helper methods for assignments

	/** Parameter (LHS or RHS) of an assignment. */
	struct AssignParam {
		boogie::Expr::Ref bgExpr; // Boogie expression
		TypePointer type; // Type
		Expression const* expr; // Original expression (not all kinds of assignments require it)
	};

	/** Result of an assignment (including side effects). */
	struct AssignResult {
		std::list<boogie::Decl::Ref> newDecls;
		std::list<boogie::Stmt::Ref> newStmts;
		std::list<boogie::Expr::Ref> ocs;
	};

	/**
	 * Helper function to make an assignment, handling all conversions and side effects.
	 * Besides Solidit Assignment expressions, it is used for assigning return parameters,
	 * initial values, etc.
	 */
	static
	AssignResult makeAssign(AssignParam lhs, AssignParam rhs, langutil::Token op, ASTNode const* assocNode, BoogieContext& context);

private:
	static
	void makeAssignInternal(AssignParam lhs, AssignParam rhs, langutil::Token op, ASTNode const* assocNode,
			BoogieContext& context, AssignResult& result);

	static
	void makeTupleAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode,
			BoogieContext& context, AssignResult& result);

	static
	void makeStructAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode,
			BoogieContext& context, AssignResult& result);

	static
	void makeArrayAssign(AssignParam lhs, AssignParam rhs, ASTNode const* assocNode,
			BoogieContext& context, AssignResult& result);

	static
	void makeBasicAssign(AssignParam lhs, AssignParam rhs, langutil::Token op, ASTNode const* assocNode,
			BoogieContext& context, AssignResult& result);

	/**
	 * Helper method to lift a conditional by pushing member accesses inside.
	 * E.g., ite(c, t, e).x becomes ite(c, t.x, e.x)
	 */
	static
	boogie::Expr::Ref liftCond(boogie::Expr::Ref expr);

	/** Checks if any sum shadow variable has to be updated based on the lhs. */
	static
	std::list<boogie::Stmt::Ref> checkForSums(boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, BoogieContext& context);

	static
	void deepCopyStruct(StructDefinition const* structDef,
				boogie::Expr::Ref lhsBase, boogie::Expr::Ref rhsBase, DataLocation lhsLoc, DataLocation rhsLoc,
				ASTNode const* assocNode, BoogieContext& context, AssignResult& result);

	// Local storage pointers
public:
	/* Result of packing a local storage pointer into an array. **/
	struct PackResult {
		boogie::Decl::Ref ptr; // Resulting pointer
		std::list<boogie::Stmt::Ref> stmts; // Side effects of packing (filling the pointer array)
	};

	/** Packs an expression into a local storage pointer. */
	static
	PackResult packToLocalPtr(Expression const* expr, boogie::Expr::Ref bgExpr, BoogieContext& context);

	/** Unpacks a local storage pointer into an access to storage. */
	static
	boogie::Expr::Ref unpackLocalPtr(Expression const* ptrExpr, boogie::Expr::Ref ptrBgExpr, BoogieContext& context);

private:
	/**
	 * Packs a path to a local storage into an array. E.g., 'ss[i].t' becomes [2, i, 3] if
	 *  'ss' is the 2nd state var and 't' is the 3rd member.
	 */
	static
	void packInternal(Expression const* expr, boogie::Expr::Ref bgExpr, StructType const* structType, BoogieContext& context, PackResult& result);

	/**
	 * Unpacks an array into a tree of paths (conditional) to local storage (opposite of packing).
	 * E.g., ite(arr[0] == 0, statevar1, ite(arr[0] == 1, statevar2, ...
	 */
	static
	boogie::Expr::Ref unpackInternal(Expression const* ptrExpr, boogie::Expr::Ref ptrBgExpr, Declaration const* decl, int depth, boogie::Expr::Ref base, BoogieContext& context);

};

}
}
