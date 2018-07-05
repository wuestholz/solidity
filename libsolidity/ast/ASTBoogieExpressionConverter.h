#pragma once

#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/ast/BoogieAst.h>

namespace dev
{
namespace solidity
{

/**
 * Converter from Solidity expressions to Boogie expressions.
 */
class ASTBoogieExpressionConverter : private ASTConstVisitor
{
private:
	// Helper variables to pass information between the visit methods
	const smack::Expr* currentExpr;
	const smack::Expr* currentAddress;
	const smack::Expr* currentValue;
	bool isGetter;

	// Converting expressions might result in new statements and declarations
	// due to differences between Solidity and Boogie
	std::vector<smack::Stmt const*> newStatements;
	std::list<smack::Decl*> newDecls;

	const smack::Expr* getArrayLength(const smack::Expr* expr);


public:

	/**
	 * Result of converting a Solidity expression to a Boogie expression.
	 * Due to differences between Solidity and Boogie, the result might
	 * introduce new statements and declarations as well.
	 */
	struct Result
	{
		const smack::Expr* expr;
		std::vector<smack::Stmt const*> newStatements;
		std::list<smack::Decl*> newDecls;

		Result(const smack::Expr* expr, std::vector<smack::Stmt const*> newStatements, std::list<smack::Decl*> newDecls)
			:expr(expr), newStatements(newStatements), newDecls(newDecls) {}
	};


	ASTBoogieExpressionConverter();

	/**
	 * Convert a Solidity Expression into a Boogie expression. As a side
	 * effect, the conversion might introduce new statements and declarations
	 * (included in the result).
	 */
	Result convert(Expression const& _node);

	bool visit(Conditional const& _node) override;
	bool visit(Assignment const& _node) override;
	bool visit(TupleExpression const& _node) override;
	bool visit(UnaryOperation const& _node) override;
	bool visit(BinaryOperation const& _node) override;
	bool visit(FunctionCall const& _node) override;
	bool visit(NewExpression const& _node) override;
	bool visit(MemberAccess const& _node) override;
	bool visit(IndexAccess const& _node) override;
	bool visit(Identifier const& _node) override;
	bool visit(ElementaryTypeNameExpression const& _node) override;
	bool visit(Literal const& _node) override;

	bool visitNode(ASTNode const&) override;

};

}
}
