/* 
    SQLTools is a tool for Oracle database developers and DBAs.
    Copyright (C) 1997-2014 Aleksey Kochetov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#pragma once

    class SessionCache;
    struct AutocompleteTemplateParams;
    typedef std::shared_ptr<SessionCache> SessionCachePtr;

    namespace OpenEditor
    {
        class Template;
        typedef std::shared_ptr<Template> TemplatePtr;
    }

    namespace ObjectTree 
    {
        class TreeNodePool;
        typedef std::shared_ptr<TreeNodePool> TreeNodePoolPtr;
        struct ObjectDescriptor;
    };


class SessionCache
{
private:
    SessionCache ();
public:

    ~SessionCache ();

    static SessionCachePtr GetSessionCache ();

    static ObjectTree::TreeNodePoolPtr GetObjectPool ()                    { return GetSessionCache()->m_objectPool; }
    static OpenEditor::TemplatePtr     GetAutocompleteTemplate ()          { return GetSessionCache()->m_autocompleteTemplate; }
    static OpenEditor::TemplatePtr     GetAutocompleteSubobjectTemplate (const string& object, const string& alias, const AutocompleteTemplateParams&);
    static OpenEditor::TemplatePtr     GetAutocompleteTemplateForSchema (const std::string& owner);

    static void OnConnect    ();
    static void OnDisconnect ();
    static void Reload       (bool force);
    static void InitIfNecessary ();

    static bool FindCachedDescriptor  (const std::string& input, ObjectTree::ObjectDescriptor&);
    static void StoreCachedDescriptor (const std::string& input, const ObjectTree::ObjectDescriptor&);

private:
    static SessionCachePtr m_sessionCache;
    static string m_cookie;
    static const int k_schemaCacheMaxSize = 8;

    ObjectTree::TreeNodePoolPtr m_objectPool;
    OpenEditor::TemplatePtr     m_autocompleteTemplate;
    unsigned char               m_lastTypeFilterMask = 0xFF;

    std::map<std::string, OpenEditor::TemplatePtr> m_schemaTemplateCache;
    std::list<std::string>          m_schemaTemplateLruList;

    std::map<std::string, ObjectTree::ObjectDescriptor> m_objectDescriptorCache;

    void doReloadObjectCache (bool force);
    void resetLanguageKeywordMap ();

    friend struct SrvBkLoadObjectCache;
    friend struct SrvBkLoadSchemaObjectCache;
};

