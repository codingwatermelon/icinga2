/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2018 Icinga Development Team (https://icinga.com/)      *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "config/configitembuilder.hpp"
#include "base/configtype.hpp"
#include <sstream>

using namespace icinga;

ConfigItemBuilder::ConfigItemBuilder(const DebugInfo& debugInfo)
	: m_Abstract(false), m_DefaultTmpl(false), m_IgnoreOnError(false)
{
	m_DebugInfo = debugInfo;
}

void ConfigItemBuilder::SetType(const Type::Ptr& type)
{
	ASSERT(type);
	m_Type = type;
}

void ConfigItemBuilder::SetName(const String& name)
{
	m_Name = name;
}

void ConfigItemBuilder::SetAbstract(bool abstract)
{
	m_Abstract = abstract;
}

void ConfigItemBuilder::SetScope(const Dictionary::Ptr& scope)
{
	m_Scope = scope;
}

void ConfigItemBuilder::SetZone(const String& zone)
{
	m_Zone = zone;
}

void ConfigItemBuilder::SetPackage(const String& package)
{
	m_Package = package;
}

void ConfigItemBuilder::AddExpression(Expression *expr)
{
	m_Expressions.emplace_back(expr);
}

void ConfigItemBuilder::SetFilter(const std::shared_ptr<Expression>& filter)
{
	m_Filter = filter;
}

void ConfigItemBuilder::SetDefaultTemplate(bool defaultTmpl)
{
	m_DefaultTmpl = defaultTmpl;
}

void ConfigItemBuilder::SetIgnoreOnError(bool ignoreOnError)
{
	m_IgnoreOnError = ignoreOnError;
}

ConfigItem::Ptr ConfigItemBuilder::Compile()
{
	if (!m_Type) {
		std::ostringstream msgbuf;
		msgbuf << "The type of an object must be specified";
		BOOST_THROW_EXCEPTION(ScriptError(msgbuf.str(), m_DebugInfo));
	}

	auto *ctype = dynamic_cast<ConfigType *>(m_Type.get());

	if (!ctype) {
		std::ostringstream msgbuf;
		msgbuf << "The type '" + m_Type->GetName() + "' cannot be used for config objects";
		BOOST_THROW_EXCEPTION(ScriptError(msgbuf.str(), m_DebugInfo));
	}

	if (m_Name.FindFirstOf("!") != String::NPos) {
		std::ostringstream msgbuf;
		msgbuf << "Name for object '" << m_Name << "' of type '" << m_Type->GetName() << "' is invalid: Object names may not contain '!'";
		BOOST_THROW_EXCEPTION(ScriptError(msgbuf.str(), m_DebugInfo));
	}

	std::vector<std::unique_ptr<Expression> > exprs;

	Array::Ptr templateArray = new Array({ m_Name });

	exprs.emplace_back(new SetExpression(MakeIndexer(ScopeThis, "templates"), OpSetAdd,
		std::unique_ptr<LiteralExpression>(new LiteralExpression(templateArray)), m_DebugInfo));

#ifdef I2_DEBUG
	if (!m_Abstract) {
		bool foundDefaultImport = false;

		for (const std::unique_ptr<Expression>& expr : m_Expressions) {
			if (dynamic_cast<ImportDefaultTemplatesExpression *>(expr.get())) {
				foundDefaultImport = true;
				break;
			}
		}

		ASSERT(foundDefaultImport);
	}
#endif /* I2_DEBUG */

	auto *dexpr = new DictExpression(std::move(m_Expressions), m_DebugInfo);
	dexpr->MakeInline();
	exprs.emplace_back(dexpr);

	std::shared_ptr<DictExpression> exprl = std::make_shared<DictExpression>(std::move(exprs), m_DebugInfo);
	exprl->MakeInline();

	return new ConfigItem(m_Type, m_Name, m_Abstract, exprl, m_Filter,
		m_DefaultTmpl, m_IgnoreOnError, m_DebugInfo, m_Scope, m_Zone, m_Package);
}

