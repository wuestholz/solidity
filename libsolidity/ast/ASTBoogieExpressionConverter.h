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
	ContractDefinition const* m_currentContract;

	// Helper variables to pass information between the visit methods
	boogie::Expr::Ref m_currentExpr;
	boogie::Expr::Ref m_currentAddress;
	boogie::Expr::Ref m_currentMsgValue;
	bool m_isGetter;
	bool m_isLibraryCall;
	bool m_isLibraryCallStatic;

	// Converting expressions might result in new statements and declarations
	// due to differences between Solidity and Boogie
	std::vector<boogie::Stmt::Ref> m_newStatements;
	std::list<boogie::Decl::Ref> m_newDecls;
	std::list<boogie::Decl::Ref> m_newConstants;
	// Type checking conditions
	std::list<boogie::Expr::Ref> m_tccs;
	// Overflow conditions
	std::list<boogie::Expr::Ref> m_ocs;

	// Helper method to get the length of an array (currently only works for 1D arrays)
	boogie::Expr::Ref getArrayLength(boogie::Expr::Ref expr, ASTNode const& associatedNode);

	// Helper method to create an assignment
	void createAssignment(Expression const& originalLhs, boogie::Expr::Ref lhs, boogie::Expr::Ref rhs);

	// Helper method to create struct assignment
	void createStructAssignment(Assignment const& _node, boogie::Expr::Ref lhsExpr, boogie::Expr::Ref rhsExpr);

	// Helper method for recursive deep copy between structures
	void deepCopyStruct(Assignment const& _node, StructDefinition const* structDef,
			boogie::Expr::Ref lhs, boogie::Expr::Ref rhs, DataLocation lhsLoc, DataLocation rhsLoc);

	// Helper method to get the length of an array
	boogie::Expr::Ref getSumShadowVar(ASTNode const* node);

	// Helper method to add a type checking condition for an expression with a given type
	void addTCC(boogie::Expr::Ref expr, TypePointer tp);

	// Helper method to add a side effect (statement)
	void addSideEffect(boogie::Stmt::Ref stmt);

	// Helper method to add a side effects (statement)
	void addSideEffects(std::vector<boogie::Stmt::Ref> const& stmts) { for (auto stmt: stmts) addSideEffect(stmt); }

	// Helper methods for the different scenarios for function calls
	void functionCallConversion(FunctionCall const& _node);
	boogie::Decl::Ref newStruct(StructDefinition const* structDef, std::string id);
	void functionCallNewStruct(FunctionCall const& _node, StructDefinition const* structDef, std::vector<boogie::Expr::Ref> const& args);
	void functionCallReduceBalance(boogie::Expr::Ref msgValue);
	void functionCallRevertBalance(boogie::Expr::Ref msgValue);
	void functionCallSum(FunctionCall const& _node);
	void functionCallOld(FunctionCall const& _node, std::vector<boogie::Expr::Ref> const& args);

public:

	/**
	 * Result of converting a Solidity expression to a Boogie expression.
	 * Due to differences between Solidity and Boogie, the result might
	 * introduce new statements and declarations as well.
	 */
	struct Result
	{
		boogie::Expr::Ref expr;
		std::vector<boogie::Stmt::Ref> newStatements;
		std::list<boogie::Decl::Ref> newDecls;
		std::list<boogie::Decl::Ref> newConstants;
		std::list<boogie::Expr::Ref> tccs; // Type checking conditions
		std::list<boogie::Expr::Ref> ocs;  // Overflow conditions

		Result(boogie::Expr::Ref expr,
				std::vector<boogie::Stmt::Ref> const& newStatements,
				std::list<boogie::Decl::Ref> const& newDecls,
				std::list<boogie::Decl::Ref> const& newConstants,
				std::list<boogie::Expr::Ref> const& tccs,
				std::list<boogie::Expr::Ref> const& ocs)
			:expr(expr), newStatements(newStatements), newDecls(newDecls),
				newConstants(newConstants), tccs(tccs), ocs(ocs) {}
	};

	/**
	 * Create a new instance with a given context and an optional location used for reporting errors.
	 */
	ASTBoogieExpressionConverter(BoogieContext& context, ContractDefinition const* currentContract);

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
