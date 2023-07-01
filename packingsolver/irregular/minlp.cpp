#include "packingsolver/irregular/minlp.hpp"
#include "packingsolver/irregular/minlp_model.hpp"

#if AMPL_FOUND
#include "ampl/ampl.h"
#endif

using namespace packingsolver;
using namespace packingsolver::irregular;

#if AMPL_FOUND
class OutputHandler : public ampl::OutputHandler
{
public:
    void output(ampl::output::Kind kind, const char* output)
    {
        (void)kind;
        (void)output;
        std::cout << output << std::endl;
        //if (kind == ampl::output::SOLVE)
        //    std::printf("Solver: %s\n", output);
    }
};
#endif

MinlpOutput irregular::minlp(
        const Instance& instance,
        MinlpOptionalParameters parameters)
{
    MinlpOutput output(instance);

#if AMPL_FOUND

    // Create an AMPL instance.
    // bin2cpp --file=minlp_model.mod --output=. --headerfile=minlp_model.hpp --namespace=packingsolver --identifier=MinlpModel --override
    ampl::AMPL ampl;
    const File& resource = getMinlpModelFile();
    const char* buffer = resource.getBuffer();
    ampl.eval(buffer);

    // AMPL book
    // https://ampl.com/wp-content/uploads/BOOK.pdf
    // Set of sets in AMPL?
    // https://groups.google.com/g/ampl/c/6IWfO6HjYuo

    std::vector<ItemTypeId> ampl2ps;
    ampl::DataFrame df_circles(1, ampl::StringArgs("CIRCLES", "rc"));
    for (ItemTypeId item_type_id = 0; item_type_id < instance.number_of_item_types(); ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.shape_type() == ShapeType::Circle) {
            LengthDbl r = distance(
                    item_type.shapes.front().shape.elements.front().center,
                    item_type.shapes.front().shape.elements.front().start);
            for (ItemPos c = 0; c < item_type.copies; ++c) {
                ampl2ps.push_back(item_type_id);
                df_circles.addRow(ampl2ps.size(), r);
            }
        }
    }

    // Bin height.
    LengthDbl xi0 = 0;
    LengthDbl xi1 = 0;
    LengthDbl yi0 = 0;
    LengthDbl yi1 = 0;
    auto bin_elements = instance.bin_type(0).shape.elements;
    if (bin_elements[0].start.y != bin_elements[0].end.y) {
        xi0 = std::min(bin_elements[1].start.x, bin_elements[1].end.x);
        xi1 = std::max(bin_elements[1].start.x, bin_elements[1].end.x);
        yi0 = std::min(bin_elements[0].start.y, bin_elements[0].end.y);
        yi1 = std::max(bin_elements[0].start.y, bin_elements[0].end.y);
    } else {
        xi0 = std::min(bin_elements[0].start.x, bin_elements[0].end.x);
        xi1 = std::max(bin_elements[0].start.x, bin_elements[0].end.x);
        yi0 = std::min(bin_elements[1].start.y, bin_elements[1].end.y);
        yi1 = std::max(bin_elements[1].start.y, bin_elements[1].end.y);
    }
    (void)xi0;
    (void)xi1;
    ampl::Parameter ampl_param_hi = ampl.getParameter("hi");
    ampl_param_hi.set(yi1 - yi0);

    // Number of circles.
    ampl::Parameter ampl_param_nc = ampl.getParameter("nc");
    ampl_param_nc.set(ampl2ps.size());

    // Circle radius.
    ampl.setData(df_circles);

    // Hide AMPL standard output.
    OutputHandler output_handler;
    ampl.setOutputHandler(&output_handler);

    if (!parameters.output_nl_path.empty()) {
        ampl.eval("write \"g" + parameters.output_nl_path + "\";");
    }

    // Solve.
    ampl.solve();

    // If the solution is infeasible, stop.
    auto rc = ampl.getValue("solve_result_num").dbl();
    if (rc >= 200)
        return output;

    // Retrieve solution.
    Solution solution(instance);
    auto xc = ampl.getVariable("XC").getValues();
    auto yc = ampl.getVariable("YC").getValues();
    auto it_xc = xc.begin();
    auto it_yc = yc.begin();
    BinPos bin_pos = solution.add_bin(0, 1);
    for (ItemPos pos = 0; pos < (ItemPos)ampl2ps.size(); ++pos, ++it_xc, ++it_yc) {
        solution.add_item(
                bin_pos,
                ampl2ps[pos],
                {(*it_xc)[1].dbl(), (*it_yc)[1].dbl()},
                0.0);
    }
    std::stringstream ss;
    output.solution_pool.add(solution, ss, parameters.info);

#endif

    return output;
}
