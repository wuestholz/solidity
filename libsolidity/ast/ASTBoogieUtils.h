#pragma once

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/BoogieAst.h>
#include <string>

namespace dev
{
namespace solidity
{
class ASTBoogieUtils
{
public:
	static const std::string SOLIDITY_ADDRESS_TYPE;
	static const std::string BOOGIE_ADDRESS_TYPE;
	static const std::string SOLIDITY_BALANCE;
	static const std::string BOOGIE_BALANCE;
	static const std::string SOLIDITY_MSG;
	static const std::string SOLIDITY_SENDER;
	static const std::string BOOGIE_MSG_SENDER;
	static const std::string SOLIDITY_TRANSFER;
	static const std::string BOOGIE_TRANSFER;
	static const std::string SOLIDITY_THIS;
	static const std::string BOOGIE_THIS;
	static const std::string SOLIDITY_ASSERT;
	static const std::string VERIFIER_MAIN;
	static const std::string BOOGIE_CONSTRUCTOR;

	static smack::ProcDecl* createTransferProc();

	/**
	 * Maps a declaration name into a name in Boogie
	 */
	static std::string mapDeclName(Declaration const& decl);

	static std::string mapType(TypePointer tp, ASTNode const& _associatedNode);
};

}
}
