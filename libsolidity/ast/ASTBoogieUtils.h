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
	static std::string const BOOGIE_BALANCE;
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
	static std::string const BOOGIE_MSG_SENDER;
	static std::string const BOOGIE_MSG_VALUE;

	// Error handling
	static std::string const SOLIDITY_ASSERT;
	static std::string const SOLIDITY_REQUIRE;
	static std::string const SOLIDITY_REVERT;
	// no constant required for 'throw' because it is a separate statement

	// Boogie types
	static std::string const BOOGIE_INT_CONST_TYPE;
	static std::string const ERR_TYPE;

	// Other identifiers
	static std::string const BOOGIE_THIS;
	static std::string const VERIFIER_MAIN;
	static std::string const VERIFIER_SUM;
	static std::string const BOOGIE_CONSTRUCTOR;
	static std::string const BOOGIE_LENGTH;
	static std::string const BOOGIE_SUM;
	static std::string const BOOGIE_ZERO_ADDRESS;
	static std::string const SOLIDITY_NOW;
	static std::string const BOOGIE_NOW;
	static std::string const VERIFIER_OVERFLOW;

	// Return this expression as an identifier when something cannot be evaluated
	static std::string const ERR_EXPR;

	static std::string const BOOGIE_STOR;
	static std::string const BOOGIE_MEM;

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

	/**
	 * Data locations as strings
	 */
	static
	std::string dataLocToStr(DataLocation loc);

	/**
	 * Map a declaration name to a name in Boogie
	 */
	static
	std::string mapDeclName(Declaration const& decl);

	/**
	 * Map a structure member with a given data location
	 * to a name in Boogie
	 */
	static
	std::string mapStructMemberName(Declaration const& decl, DataLocation loc);

	static
	std::string getConstructorName(ContractDefinition const* contract);

	/**
	 * Get address type for a struct with a given data location
	 */
	static
	boogie::TypeDeclRef getStructAddressType(StructDefinition const* structDef, DataLocation loc);

	/**
	 * Map a Solidity type to a Boogie type
	 */
	static
	boogie::TypeDeclRef toBoogieType(TypePointer tp, ASTNode const* _associatedNode, BoogieContext& context);

	static
	boogie::TypeDeclRef mappingType(boogie::TypeDeclRef keyType, boogie::TypeDeclRef valueType);

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

	static
	bool isStateVar(Declaration const *decl);
};

}
}
