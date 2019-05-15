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
	static const std::string SOLIDITY_BALANCE;
	static const std::string BOOGIE_BALANCE;
	static const std::string SOLIDITY_TRANSFER;
	static const std::string BOOGIE_TRANSFER;
	static const std::string SOLIDITY_SEND;
	static const std::string BOOGIE_SEND;
	static const std::string SOLIDITY_CALL;
	static const std::string BOOGIE_CALL;

	// Contract related identifiers
	static const std::string SOLIDITY_SUPER;
	static const std::string SOLIDITY_THIS;

	// Identifiers related to 'msg'
	static const std::string SOLIDITY_SENDER;
	static const std::string SOLIDITY_VALUE;
	static const std::string BOOGIE_MSG_SENDER;
	static const std::string BOOGIE_MSG_VALUE;

	// Error handling
	static const std::string SOLIDITY_ASSERT;
	static const std::string SOLIDITY_REQUIRE;
	static const std::string SOLIDITY_REVERT;
	// no constant required for 'throw' because it is a separate statement

	// Boogie types
	static const std::string BOOGIE_ADDRESS_TYPE;
	static const std::string BOOGIE_STRING_TYPE;
	static const std::string BOOGIE_BOOL_TYPE;
	static const std::string BOOGIE_INT_TYPE;
	static const std::string BOOGIE_INT_CONST_TYPE;
	static const std::string ERR_TYPE;

	// Other identifiers
	static const std::string BOOGIE_THIS;
	static const std::string VERIFIER_MAIN;
	static const std::string VERIFIER_SUM;
	static const std::string BOOGIE_CONSTRUCTOR;
	static const std::string BOOGIE_LENGTH;
	static const std::string BOOGIE_SUM;
	static const std::string BOOGIE_ZERO_ADDRESS;
	static const std::string SOLIDITY_NOW;
	static const std::string BOOGIE_NOW;
	static const std::string VERIFIER_OVERFLOW;

	// Return this expression as an identifier when something cannot be evaluated
	static const std::string ERR_EXPR;

	static const std::string BOOGIE_STOR;
	static const std::string BOOGIE_MEM;

	/**
	 * Create the procedure corresponding to address.transfer()
	 */
	static
	boogie::ProcDeclRef createTransferProc(BoogieContext& context);

	/**
	 * Create the procedure corresponding to address.call()
	 */
	static
	boogie::ProcDeclRef createCallProc(BoogieContext& context);

	/**
	 * Create the procedure corresponding to address.send()
	 */
	static
	boogie::ProcDeclRef createSendProc(BoogieContext& context);

	static
	std::string dataLocToStr(DataLocation loc);

	/**
	 * Map a declaration name to a name in Boogie
	 */
	static
	std::string mapDeclName(Declaration const& decl);

	static
	std::string mapStructMemberName(Declaration const& decl, DataLocation loc);

	static
	std::string getConstructorName(ContractDefinition const* contract);

	static
	std::string getStructAddressType(StructDefinition const* structDef, DataLocation loc);

	/**
	 * Map a Solidity type to a Boogie type
	 */
	static
	std::string mapType(TypePointer tp, ASTNode const* _associatedNode, BoogieContext& context);

	/**
	 * Return Boogie's BV type of given size.
	 */
	static
	std::string boogieBVType(unsigned n);

	/**
	 * Create attributes for original source location and message
	 */
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

	/**
	 * Check if a type can be represented with bitvectors
	 */
	static bool isBitPreciseType(TypePointer type);

	/**
	 * Get the number of bits required to represent type
	 */
	static unsigned getBits(TypePointer type);

	/**
	 * Check if the type is signed
	 */
	static bool isSigned(TypePointer type);

	/**
	 * Check if bitvector type conversion from the implicit type conversion from exprType
	 * to targetType. If yes, the conversion expression is returned, otherwise expr is
	 * returned unchanged. For example, if exprType is uint32 and targetType is uint40,
	 * then we return an extension of 8 bits to expr.
	 */
	static
	boogie::Expr::Ref checkImplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);


	static
	boogie::Expr::Ref checkExplicitBvConversion(boogie::Expr::Ref expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);

	/**
	 * Get the type checking condition for an expression with a given type.
	 * Depending on the context, the returned TCC can be assumed or asserted.
	 */
	static
	boogie::Expr::Ref getTCCforExpr(boogie::Expr::Ref expr, TypePointer tp);
};

}
}
