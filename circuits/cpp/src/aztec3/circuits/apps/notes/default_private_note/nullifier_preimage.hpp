#pragma once

#include "aztec3/utils/types/circuit_types.hpp"
#include "aztec3/utils/types/convert.hpp"
#include "aztec3/utils/types/native_types.hpp"

#include <barretenberg/barretenberg.hpp>

namespace aztec3::circuits::apps::notes {

using aztec3::utils::types::CircuitTypes;
using aztec3::utils::types::NativeTypes;

template <typename NCT> struct DefaultPrivateNoteNullifierPreimage {
    using fr = typename NCT::fr;
    using boolean = typename NCT::boolean;

    fr commitment;
    fr owner_private_key;
    boolean is_dummy = false;

    bool operator==(DefaultPrivateNoteNullifierPreimage<NCT> const&) const = default;

    template <typename Composer>
    DefaultPrivateNoteNullifierPreimage<CircuitTypes<Composer>> to_circuit_type(Composer& composer) const
    {
        static_assert((std::is_same<NativeTypes, NCT>::value));

        // Capture the composer:
        auto to_ct = [&](auto& e) { return aztec3::utils::types::to_ct(composer, e); };

        DefaultPrivateNoteNullifierPreimage<CircuitTypes<Composer>> preimage = {
            to_ct(commitment),
            to_ct(owner_private_key),
            to_ct(is_dummy),
        };

        return preimage;
    };
};

template <typename NCT> void read(uint8_t const*& it, DefaultPrivateNoteNullifierPreimage<NCT>& preimage)
{
    using serialize::read;

    read(it, preimage.commitment);
    read(it, preimage.owner_private_key);
    read(it, preimage.is_dummy);
};

template <typename NCT> void write(std::vector<uint8_t>& buf, DefaultPrivateNoteNullifierPreimage<NCT> const& preimage)
{
    using serialize::write;

    write(buf, preimage.commitment);
    write(buf, preimage.owner_private_key);
    write(buf, preimage.is_dummy);
};

template <typename NCT>
std::ostream& operator<<(std::ostream& os, DefaultPrivateNoteNullifierPreimage<NCT> const& preimage)
{
    return os << "commitment: " << preimage.commitment << "\n"
              << "owner_private_key: " << preimage.owner_private_key << "\n"
              << "is_dummy: " << preimage.is_dummy << "\n";
}

}  // namespace aztec3::circuits::apps::notes