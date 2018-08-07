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
	// Return this expression as an identifier when something cannot be evaluated
	static const std::string ERR_EXPR;

	BoogieContext& m_context;

	// When this location is given, use this to report errors, this is used for
	// invariants, as they do not have corresponding nodes in the original AST
	SourceLocation const* m_defaultLocation;

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

	// Helper method to get the length of an array (currently only works for 1D arrays)
	const smack::Expr* getArrayLength(const smack::Expr* expr, ASTNode const& associatedNode);

	// Helper method to create an assignment
	void createAssignment(Expression const& originalLhs, smack::Expr const *lhs, smack::Expr const* rhs);

	// Helper method to transform a select to an update
	smack::Expr const* selectToUpdate(smack::SelExpr const* sel, smack::Expr const* value);

	// Helper method to get the length of an array
	const smack::Expr* getSumShadowVar(ASTNode const* node);

	// Helper methods to report errors and warnings
	void reportError(SourceLocation const& location, std::string const& description);
	void reportWarning(SourceLocation const& location, std::string const& description);

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

		Result(const smack::Expr* expr, std::vector<smack::Stmt const*> newStatements,
				std::list<smack::Decl*> newDecls, std::list<smack::Decl*> newConstants)
			:expr(expr), newStatements(newStatements), newDecls(newDecls), newConstants(newConstants){}
	};

	/**
	 * Create a new instance with a given context and an optional location used for reporting errors.
	 */
	ASTBoogieExpressionConverter(BoogieContext& context, SourceLocation const* defaultLocation = nullptr);

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