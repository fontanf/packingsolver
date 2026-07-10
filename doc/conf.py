# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'PackingSolver'
copyright = '2019-2024, Florian Fontan'
author = 'Florian Fontan'
release = '0.1'

html_title = project


# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['myst_parser', 'sphinx.ext.mathjax']

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# html_theme = 'sphinx_rtd_theme'
# html_theme = 'alabaster'
# html_theme = 'sphinx_book_theme'
html_theme = 'classic'
html_sidebars = { '**': ['searchbox.html', 'globaltoc.html'] }
html_theme_options = {
    'sidebarwidth': '350',
    # 'body_max_width': 'none',
}
