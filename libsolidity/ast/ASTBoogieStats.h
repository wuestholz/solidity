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

	std::map<StructDefinition const*, std::list<ContractDefinition const*>> m_contractsForStructs;

	bool hasDocTag(DocumentedAnnotation const& _annot, std::string _tag) const;

public:
	ASTBoogieStats() : m_hasModifiesSpecs(false) {}
	bool hasModifiesSpecs() const { return m_hasModifiesSpecs; }
	std::list<ContractDefinition const*> contractsForStruct(StructDefinition const* structDef) { return m_contractsForStructs[structDef]; }

	bool visit(ContractDefinition const& _node) override;
	bool visit(FunctionDefinition const& _node) override;
};

}
}
