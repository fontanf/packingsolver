#include "common.hpp"

using namespace packingsolver::python;

void bind_rectangleguillotine(nb::module_& m);
void bind_rectangle(nb::module_& m);
void bind_box(nb::module_& m);
void bind_boxstacks(nb::module_& m);
void bind_onedimensional(nb::module_& m);
void bind_irregular(nb::module_& m);

NB_MODULE(_packingsolver, m)
{
    bind_common(m);

    nb::module_ rectangleguillotine = m.def_submodule("rectangleguillotine", "Problem type 'rectangleguillotine'.");
    bind_rectangleguillotine(rectangleguillotine);

    nb::module_ rectangle = m.def_submodule("rectangle", "Problem type 'rectangle'.");
    bind_rectangle(rectangle);

    nb::module_ box = m.def_submodule("box", "Problem type 'box'.");
    bind_box(box);

    nb::module_ boxstacks = m.def_submodule("boxstacks", "Problem type 'boxstacks'.");
    bind_boxstacks(boxstacks);

    nb::module_ onedimensional = m.def_submodule("onedimensional", "Problem type 'onedimensional'.");
    bind_onedimensional(onedimensional);

    nb::module_ irregular = m.def_submodule("irregular", "Problem type 'irregular'.");
    bind_irregular(irregular);
}
