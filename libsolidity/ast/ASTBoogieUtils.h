#pragma once

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/BoogieAst.h>
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
	static const std::string SOLIDITY_ADDRESS_TYPE;
	static const std::string BOOGIE_ADDRESS_TYPE;
	static const std::string SOLIDITY_BALANCE;
	static const std::string BOOGIE_BALANCE;
	static const std::string SOLIDITY_MSG;
	static const std::string SOLIDITY_SENDER;
	static const std::string SOLIDITY_VALUE;
	static const std::string BOOGIE_MSG_SENDER;
	static const std::string BOOGIE_MSG_VALUE;
	static const std::string SOLIDITY_TRANSFER;
	static const std::string BOOGIE_TRANSFER;
	static const std::string SOLIDITY_CALL;
	static const std::string BOOGIE_CALL;
	static const std::string SOLIDITY_THIS;
	static const std::string BOOGIE_THIS;
	static const std::string SOLIDITY_ASSERT;
	static const std::string VERIFIER_MAIN;
	static const std::string BOOGIE_CONSTRUCTOR;
	static const std::string BOOGIE_LENGTH;

	/**
	 * Create the procedure corresponding to address.transfer()
	 */
	static smack::ProcDecl* createTransferProc();

	/**
	 * Create the procedure corresponding to address.call()
	 */
	static smack::ProcDecl* createCallProc();

	/**
	 * Map a declaration name to a name in Boogie
	 */
	static std::string mapDeclName(Declaration const& decl);

	/**
	 * Map a Solidity type to a Boogie type
	 */
	static std::string mapType(TypePointer tp, ASTNode const& _associatedNode);
};

}
}
