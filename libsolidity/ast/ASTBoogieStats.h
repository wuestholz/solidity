#pragma once

#include <libsolidity/ast/ASTVisitor.h>

namespace dev
{
namespace solidity
{
/**
 * Helper class for collecting statistics about the AST
 */
class ASTBoogieStats : public ASTConstVisitor
{
private:
	bool m_hasModifiesSpecs;

	bool hasDocTag(DocumentedAnnotation const& _annot, std::string _tag) const;

public:
	ASTBoogieStats() : m_hasModifiesSpecs(false) {}
	bool hasModifiesSpecs() const { return m_hasModifiesSpecs; }

	bool visit(FunctionDefinition const& _node) override;
};

}
}
