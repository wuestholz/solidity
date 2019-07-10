#pragma once

#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/ast/BoogieContext.h>

namespace dev
{
namespace solidity
{

/**
 * Converter from Solidity AST to Boogie AST.
 */
class ASTBoogieConverter : private ASTConstVisitor
{
private:
	BoogieContext& m_context;

	// Helper variables to pass information between the visit methods
	ContractDefinition const* m_currentContract;
	FunctionDefinition const* m_currentFunc; // Function currently being processed
	unsigned long m_currentModifier; // Index of the current modifier being processed

	// Collect local variable declarations (Boogie requires them at the beginning of the function).
	std::vector<boogie::Decl::Ref> m_localDecls;

	// Current block(s) where statements are appended, stack is needed due to nested blocks
	std::stack<boogie::Block::Ref> m_currentBlocks;

	// Return statement in Solidity is mapped to an assignment to the return
	// variables in Boogie, which is described by currentRet
	boogie::Expr::Ref m_currentRet;
	// Current label to jump to when encountering a return. This is needed because modifiers
	// are inlined and their returns should not jump out of the whole function.
	std::string m_currentReturnLabel;
	int m_nextReturnLabelId;

	std::string m_currentContinueLabel;

	/**
	 * Helper method to convert an expression using the dedicated expression converter class,
	 * it also handles side-effect statements and declarations introduced by the conversion
	 */
	boogie::Expr::Ref convertExpression(Expression const& _node);

	/**
	 * Helper method to give a default value for a type.
	 */
	boogie::Expr::Ref defaultValue(TypePointer _type, BoogieContext& context);

	/**
	 * Helper method to get all Boogie IDs of a given type in the current scope.
	 */
	void getVariablesOfType(TypePointer _type, ASTNode const& _scope, std::vector<boogie::Expr::Ref>& output);

	/**
	 * Helper method to produce statement assigning a default value for a declared variable.
	 */
	bool defaultValueAssignment(VariableDeclaration const& _node, ASTNode const& _scope, std::vector<boogie::Stmt::Ref>& output);

	/**
	 * Create default constructor for a contract (it is required when there is no constructor,
	 * but state variables are initialized when declared)
	 */
	void createImplicitConstructor(ContractDefinition const& _node);

	/**
	 * Helper method to add the extra preamble that a constructor requires.
	 * Used by both regular and implicit constructors.
	 */
	void constructorPreamble(ASTNode const& _scope);

	/**
	 * Helper method to initialize a state variable based on an explicit expression or
	 * a default value
	 */
	void initializeStateVar(VariableDeclaration const& _node, ASTNode const& _scope);

	/**
	 * Helper method to parse an expression from a string with a given scope
	 */
	bool parseExpr(std::string exprStr, ASTNode const& _node, ASTNode const* _scope, BoogieContext::DocTagExpr& result);

	/**
	 * Parse expressions from documentation for a given doctag
	 */
	std::vector<BoogieContext::DocTagExpr> getExprsFromDocTags(ASTNode const& _node, DocumentedAnnotation const& _annot,
			ASTNode const* _scope, std::string _tag);

	/**
	 * Checks if contract invariants are explicitly requested (for non-public functions)
	 */
	bool includeContractInvars(DocumentedAnnotation const& _annot);

	/**
	 * Helper method to extract the variable to which the modifies specification corresponds.
	 * For example, in x[1].m, the base variable is x.
	 */
	Declaration const* getModifiesBase(Expression const* expr);

	/*
	 * Helper method to replace the base variable of an expression, e.g.,
	 * replacing 'x' in 'x[i].y'.
	 */
	boogie::Expr::Ref replaceBaseVar(boogie::Expr::Ref expr, boogie::Expr::Ref value);
	bool isBaseVar(boogie::Expr::Ref expr);

	/**
	 * Helper method to extract and add modifies specifications to a function
	 */
	void addModifiesSpecs(FunctionDefinition const& _node, boogie::ProcDeclRef procDecl);

	/**
	 * Helper method to recursively process modifiers and then finally the function body
	 */
	void processFuncModifiersAndBody();

	/**
	 * Chronological stack of scoppable nodes.
	 */
	std::stack<ASTNode const*> m_scopes;

	/** Remember the scope of the node before visiting */
	void rememberScope(ASTNode const& _node)
	{
		if (dynamic_cast<Scopable const*>(&_node))
			m_scopes.push(&_node);
	}

	/** If the node is scopable, it will be removed from the scopes stack */
	void endVisitNode(ASTNode const& _node) override
	{
		if (m_scopes.size() > 0)
			if (m_scopes.top() == &_node)
				m_scopes.pop();
	}

	/** Returns the closest scoped node */
	ASTNode const* scope() const
	{
		if (m_scopes.size() > 0)
			return m_scopes.top();
		else
			return nullptr;
	}

public:
	/**
	 * Create a new instance with a given context
	 */
	ASTBoogieConverter(BoogieContext& context);

	/**
	 * Convert a node and add it to the actual Boogie program
	 */
	void convert(ASTNode const& _node) { _node.accept(*this); }

	bool visit(SourceUnit const& _node) override;
	bool visit(PragmaDirective const& _node) override;
	bool visit(ImportDirective const& _node) override;
	bool visit(ContractDefinition const& _node) override;
	bool visit(InheritanceSpecifier const& _node) override;
	bool visit(UsingForDirective const& _node) override;
	bool visit(StructDefinition const& _node) override;
	bool visit(EnumDefinition const& _node) override;
	bool visit(EnumValue const& _node) override;
	bool visit(ParameterList const& _node) override;
	bool visit(FunctionDefinition const& _node) override;
	bool visit(VariableDeclaration const& _node) override;
	bool visit(ModifierDefinition const& _node) override;
	bool visit(ModifierInvocation const& _node) override;
	bool visit(EventDefinition const& _node) override;
	bool visit(ElementaryTypeName const& _node) override;
	bool visit(UserDefinedTypeName const& _node) override;
	bool visit(FunctionTypeName const& _node) override;
	bool visit(Mapping const& _node) override;
	bool visit(ArrayTypeName const& _node) override;
	bool visit(InlineAssembly const& _node) override;
	bool visit(Block const& _node) override;
	bool visit(PlaceholderStatement const& _node) override;
	bool visit(IfStatement const& _node) override;
	bool visit(WhileStatement const& _node) override;
	bool visit(ForStatement const& _node) override;
	bool visit(Continue const& _node) override;
	bool visit(Break const& _node) override;
	bool visit(Return const& _node) override;
	bool visit(Throw const& _node) override;
	bool visit(EmitStatement const& _node) override;
	bool visit(VariableDeclarationStatement const& _node) override;
	bool visit(ExpressionStatement const& _node) override;

	// Conversion of expressions is implemented by a separate class

	bool visitNode(ASTNode const&) override;

};

}
}
