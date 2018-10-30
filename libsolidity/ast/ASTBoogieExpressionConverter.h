#pragma once

#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/BoogieContext.h>

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

	BoogieContext& m_context;

	// Helper variables to pass information between the visit methods
	const smack::Expr* m_currentExpr;
	const smack::Expr* m_currentAddress;
	const smack::Expr* m_currentMsgValue;
	bool m_isGetter;
	bool m_isLibraryCall;
	bool m_isLibraryCallStatic;

	// Converting expressions might result in new statements and declarations
	// due to differences between Solidity and Boogie
	std::vector<smack::Stmt const*> m_newStatements;
	std::list<smack::Decl*> m_newDecls;
	std::list<smack::Decl*> m_newConstants;
	// Type checking conditions
	std::list<smack::Expr const*> m_tccs;
	// Overflow conditions
	std::list<smack::Expr const*> m_ocs;

	// Helper method to get the length of an array (currently only works for 1D arrays)
	const smack::Expr* getArrayLength(const smack::Expr* expr, ASTNode const& associatedNode);

	// Helper method to create an assignment
	void createAssignment(Expression const& originalLhs, smack::Expr const *lhs, smack::Expr const* rhs);

	// Helper method to transform a select to an update
	smack::Expr const* selectToUpdate(smack::SelExpr const* sel, smack::Expr const* value);

	// Helper method to get the length of an array
	const smack::Expr* getSumShadowVar(ASTNode const* node);

	// Helper method to add a type checking condition for an expression with a given type
	void addTCC(smack::Expr const* expr, TypePointer tp);

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
		std::list<smack::Decl*> newConstants;
		std::list<smack::Expr const*> tccs; // Type checking conditions
		std::list<smack::Expr const*> ocs;  // Overflow conditions

		Result(const smack::Expr* expr, std::vector<smack::Stmt const*> newStatements,
				std::list<smack::Decl*> newDecls, std::list<smack::Decl*> newConstants,
				std::list<smack::Expr const*> tccs, std::list<smack::Expr const*> ocs)
			:expr(expr), newStatements(newStatements), newDecls(newDecls),
			 newConstants(newConstants), tccs(tccs), ocs(ocs) {}
	};

	/**
	 * Create a new instance with a given context and an optional location used for reporting errors.
	 */
	ASTBoogieExpressionConverter(BoogieContext& context);

	/**
	 * Convert a Solidity Expression into a Boogie expression. As a side effect, the conversion might
	 * introduce new statements and declarations (included in the result).
	 */
	Result convert(Expression const& _node);

	// Only need to handle expressions
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
