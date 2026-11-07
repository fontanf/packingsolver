"""Link the sections of a page which describe another page to that page in the
table of contents.

In the pages of the solver internals, each section summarizes an algorithm,
and a hidden toctree in the section lists the page of its detailed
description:

    Tree search
    -----------

    ...

    See :doc:`the detailed description <irregular/tree_search>`.

    .. toctree::
       :hidden:

       irregular/tree_search

In the table of contents (the sidebar), the entry of such a section is
replaced by the entry of the page: clicking on it opens the page instead of
moving to the section.

The sections of these detailed descriptions ('internals/<problem
type>/<algorithm>') are not listed in the table of contents, only their own
sub-pages, such as the engines of the irregular local search.
"""

from docutils import nodes
from sphinx import addnodes


def replace_section_entries(toc):
    for list_item in list(toc.findall(nodes.list_item)):
        # A section entry: a reference, then the list of its sub-entries.
        if len(list_item.children) != 2:
            continue
        sub_entries = list_item.children[1]
        if not isinstance(sub_entries, nodes.bullet_list):
            continue
        if len(sub_entries.children) != 1:
            continue
        toctree = sub_entries.children[0]
        if not isinstance(toctree, addnodes.toctree):
            continue
        if len(toctree["entries"]) != 1:
            continue
        list_item.parent.replace(list_item, toctree)


def keep_sub_pages_only(toc):
    # The entry of the title of the page, and the list of its sub-entries.
    for title_item in toc.children:
        if len(title_item.children) != 2:
            continue
        sub_entries = title_item.children[1]
        toctrees = list(sub_entries.findall(addnodes.toctree))
        if toctrees:
            sub_entries.children = []
            for toctree in toctrees:
                sub_entries.append(toctree.deepcopy())
        else:
            title_item.remove(sub_entries)


def doctree_read(app, doctree):
    docname = app.env.docname
    toc = app.env.tocs.get(docname)
    if toc is None:
        return
    replace_section_entries(toc)
    if docname.startswith("internals/") and docname.count("/") >= 2:
        keep_sub_pages_only(toc)


def setup(app):
    # After the table of contents of the page is built (priority 500).
    app.connect("doctree-read", doctree_read, priority=600)
    # The tables of contents are built when the pages are read: increase the
    # version when this extension changes, so that all the pages are read
    # again.
    return {
        "env_version": 2,
        "parallel_read_safe": True,
        "parallel_write_safe": True,
    }
