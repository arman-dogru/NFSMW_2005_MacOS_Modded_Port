#include <game/speedbreaker_mass.h>
#include <cassert>
#include <limits>

int main()
{
    using game::speedbreaker::ActivationMass;
    assert(ActivationMass(3000, 2) == 3000); // stock Lexus-sized car
    assert(ActivationMass(3000, 1) == 1500);
    assert(ActivationMass(3000, 0) == 0);
    assert(ActivationMass(3000, 1000) == 1500000);
    assert(ActivationMass(3000, 1000000) == 1000000000); // mass cap
    assert(ActivationMass(0, 1000) == 0); // clear remains clear
    assert(ActivationMass(-1, 1000) == -1);
    for (double invalid : {-1.0, 1000001.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        assert(ActivationMass(3000, invalid) == 3000);
    assert(ActivationMass(std::numeric_limits<double>::max(), 1000000) == 1000000000);
    assert(std::isnan(ActivationMass(std::numeric_limits<double>::quiet_NaN(), 1000)));
}
