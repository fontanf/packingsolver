#include "packingsolver/irregular/nlp.hpp"

#if KNITRO_FOUND
#include "knitrocpp/knitro.hpp"
#endif

using namespace packingsolver;
using namespace packingsolver::irregular;

NlpOutput irregular::nlp(
        const Instance& instance,
        NlpOptionalParameters parameters)
{
    NlpOutput output(instance);

#if KNITRO_FOUND

#endif

    return output;
}
