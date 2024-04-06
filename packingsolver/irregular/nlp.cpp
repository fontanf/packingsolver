#include "packingsolver/irregular/nlp.hpp"

#if KNITRO_FOUND
#include "knitrocpp/knitro.hpp"
#endif

using namespace packingsolver;
using namespace packingsolver::irregular;

const NlpOutput irregular::nlp(
        const Instance& instance,
        const NlpParameters& parameters)
{
    NlpOutput output(instance);
    (void)parameters;

#if KNITRO_FOUND

#endif

    return output;
}
