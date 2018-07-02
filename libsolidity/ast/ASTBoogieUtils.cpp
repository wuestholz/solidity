#include <libsolidity/ast/ASTBoogieUtils.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{
const string ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE = "address";
const string ASTBoogieUtils::BOOGIE_ADDRESS_TYPE = "address_t";
const string ASTBoogieUtils::SOLIDITY_BALANCE = "balance";
const string ASTBoogieUtils::BOOGIE_BALANCE = "__balance";
const string ASTBoogieUtils::SOLIDITY_MSG = "msg";
const string ASTBoogieUtils::SOLIDITY_SENDER = "sender";
const string ASTBoogieUtils::BOOGIE_MSG_SENDER = "__msg_sender";
const string ASTBoogieUtils::SOLIDITY_TRANSFER = "transfer";
const string ASTBoogieUtils::BOOGIE_TRANSFER = "__transfer";
const string ASTBoogieUtils::SOLIDITY_THIS = "this";
const string ASTBoogieUtils::BOOGIE_THIS = "__this";
const string ASTBoogieUtils::SOLIDITY_ASSERT = "assert";
const string ASTBoogieUtils::VERIFIER_MAIN = "__verifier_main";
const string ASTBoogieUtils::BOOGIE_CONSTRUCTOR = "__constructor";

smack::ProcDecl* ASTBoogieUtils::createTransferProc()
{
	list<smack::Binding> transferParams;
	transferParams.push_back(make_pair(ASTBoogieUtils::BOOGIE_THIS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // 'this'
	transferParams.push_back(make_pair(ASTBoogieUtils::BOOGIE_MSG_SENDER, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE)); // 'msg.sender'
	transferParams.push_back(make_pair("amount", "int"));
	smack::Block* transferImpl = smack::Block::block();
	const smack::Expr* this_bal = smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
	const smack::Expr* sender_bal = smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_MSG_SENDER);
	const smack::Expr* amount = smack::Expr::id("amount");

	// balance[this] += amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
					smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
					smack::Expr::plus(this_bal, amount)
			)));
	// balance[msg.sender] -= amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
					smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER),
					smack::Expr::minus(sender_bal, amount)
			)));
	transferImpl->addStmt(smack::Stmt::comment("TODO: call fallback, exception handling"));
	list<smack::Block*> transferBlocks;
	transferBlocks.push_back(transferImpl);
	smack::ProcDecl* transfer = smack::Decl::procedure(ASTBoogieUtils::BOOGIE_TRANSFER,
			transferParams, list<smack::Binding>(), list<smack::Decl*>(), transferBlocks);
	// Precondition: there is enough ether to transfer
	transfer->getRequires().push_back(smack::Expr::gte(sender_bal, amount));
	// Postcondition: if sender and receiver is different (ether gets transferred)
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::neq(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER)),
					smack::Expr::eq(sender_bal, smack::Expr::minus(smack::Expr::old(sender_bal), amount))));
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::neq(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER)),
					smack::Expr::eq(this_bal, smack::Expr::plus(smack::Expr::old(this_bal), amount))));
	// Postcondition: if sender and receiver is the same (nothing happens)
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::eq(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER)),
					smack::Expr::eq(sender_bal, smack::Expr::old(sender_bal))));
	transfer->getEnsures().push_back(
			smack::Expr::impl(
					smack::Expr::eq(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER)),
					smack::Expr::eq(this_bal, smack::Expr::old(this_bal))));

	return transfer;
}
}
}
