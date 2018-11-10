#pragma once

#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/BoogieContext.h>
#include <libsolidity/parsing/Scanner.h>
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
	static const std::string SOLIDITY_ADDRESS_TYPE;
	static const std::string BOOGIE_ADDRESS_TYPE;
	static const std::string SOLIDITY_BALANCE;
	static const std::string BOOGIE_BALANCE;
	static const std::string SOLIDITY_TRANSFER;
	static const std::string BOOGIE_TRANSFER;
	static const std::string SOLIDITY_SEND;
	static const std::string BOOGIE_SEND;
	static const std::string SOLIDITY_CALL;
	static const std::string BOOGIE_CALL;
	static const std::string SOLIDITY_SUPER;

	// Identifiers related to 'msg'
	static const std::string SOLIDITY_MSG;
	static const std::string SOLIDITY_SENDER;
	static const std::string SOLIDITY_VALUE;
	static const std::string BOOGIE_MSG_SENDER;
	static const std::string BOOGIE_MSG_VALUE;

	// Error handling
	static const std::string SOLIDITY_ASSERT;
	static const std::string SOLIDITY_REQUIRE;
	static const std::string SOLIDITY_REVERT;
	// no constant required for 'throw' because it is a separate statement

	// Other identifiers
	static const std::string SOLIDITY_THIS;
	static const std::string BOOGIE_THIS;
	static const std::string VERIFIER_MAIN;
	static const std::string VERIFIER_SUM;
	static const std::string BOOGIE_CONSTRUCTOR;
	static const std::string BOOGIE_LENGTH;
	static const std::string BOOGIE_SUM;
	static const std::string BOOGIE_STRING_TYPE;
	static const std::string ERR_TYPE;
	static const std::string BOOGIE_ZERO_ADDRESS;
	static const std::string SOLIDITY_NOW;
	static const std::string BOOGIE_NOW;
	static const std::string VERIFIER_OVERFLOW;

	// Return this expression as an identifier when something cannot be evaluated
	static const std::string ERR_EXPR;

	/**
	 * Create the procedure corresponding to address.transfer()
	 */
	static smack::ProcDecl* createTransferProc(BoogieContext& context);

	/**
	 * Create the procedure corresponding to address.call()
	 */
	static smack::ProcDecl* createCallProc(BoogieContext& context);

	/**
	 * Create the procedure corresponding to address.send()
	 */
	static smack::ProcDecl* createSendProc(BoogieContext& context);

	/**
	 * Map a declaration name to a name in Boogie
	 */
	static std::string mapDeclName(Declaration const& decl);

	/**
	 * Map a Solidity type to a Boogie type
	 */
	static std::string mapType(TypePointer tp, ASTNode const& _associatedNode, BoogieContext& context);

	/**
	 * Create attributes for original source location and message
	 */
	static std::list<const smack::Attr*> createAttrs(SourceLocation const& loc, std::string const& message, Scanner const& scanner);

	/** Pair of expressions: first = result, second = correctness condition */
	typedef std::pair<smack::Expr const*, smack::Expr const*> expr_pair;

	static expr_pair encodeArithBinaryOp(BoogieContext& context, ASTNode const* associatedNode, Token::Value op, smack::Expr const* lhs, smack::Expr const* rhs, unsigned bits, bool isSigned);

	static expr_pair encodeArithUnaryOp(BoogieContext& context, ASTNode const* associatedNode, Token::Value op, smack::Expr const* subExpr, unsigned bits, bool isSigned);

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
	static smack::Expr const* checkImplicitBvConversion(smack::Expr const* expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);


	static smack::Expr const* checkExplicitBvConversion(smack::Expr const* expr, TypePointer exprType, TypePointer targetType, BoogieContext& context);

	/**
	 * Get the type checking condition for an expression with a given type.
	 * Depending on the context, the returned TCC can be assumed or asserted.
	 */
	static smack::Expr const* getTCCforExpr(smack::Expr const* expr, TypePointer tp);
};

}
}
