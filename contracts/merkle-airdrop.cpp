#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/currency.hpp>

static constexpr uint64_t token_symbol = S(4, EOS); // precision, symbol
static constexpr account_name token_contract = N(eosio.token); // token contract name

using eosio::asset;
using eosio::extended_asset;
using eosio::singleton;
using eosio::currency;


class merkle_airdrop : public eosio::contract {
public:
    // @abi action
    struct mint {
        account_name sender;
        asset amount;
        std::vector<checksum256> proof;

        EOSLIB_SERIALIZE(mint, (sender)(amount)(proof))
    };

    // @abi action
    struct setroot {
        checksum256 mroot;

        EOSLIB_SERIALIZE(setroot, (mroot))
    };

public:
    merkle_airdrop(account_name self)
        : contract(self)
        , _mroot(self, self)
    { }

    void on(mint const & act) {
        require_auth(act.sender);

        eosio_assert(_mroot.exists(), "Merkle root is not exist");
        eosio_assert(act.amount.symbol == token_symbol, "Token symbol mismatch");

        std::string leaf = eosio::name{act.sender}.to_string() + std::to_string(act.amount.amount);

        checksum256 leaf_hash;
        sha256(const_cast<char*>(leaf.data()), leaf.size(), &leaf_hash);

        eosio_assert(check_proof(leaf_hash, act.proof), "Merkle proof fail");

        currency::inline_transfer(_self, act.sender, extended_asset(act.amount, token_contract), "airdrop");
    }

    void on(setroot const & act) {
        require_auth(_self);

        eosio_assert(!_mroot.exists(), "Merkle root already exist");

        _mroot.set(act.mroot, _self);
    }

    void apply(account_name contract, account_name act) {
        if (contract != _self)
            return;

        switch (act) {
            case N(mint):
                on(eosio::unpack_action_data<mint>());
                break;
            case N(setroot):
                on(eosio::unpack_action_data<setroot>());
                break;
        }
    }

protected:
    singleton<N(mroot), checksum256> _mroot;

    char* hash_cat(const checksum256 & l, const checksum256 & r) {
        static char buf[64];
        memcpy(buf, &l.hash, 32);
        memcpy(buf + 32, &r.hash, 32);
        return buf;
    };

    bool check_proof(const checksum256 & leaf, const std::vector<checksum256> & proof) {
        checksum256 current = leaf;

        for (auto && el: proof) {
            if (std::less<checksum256>()(current, el)) {
                sha256(hash_cat(current, el), 64, &current);
            } else {
                sha256(hash_cat(el, current), 64, &current);
            }
        }

        return current == _mroot.get();
    }
};


extern  "C" {

[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    merkle_airdrop contract(receiver);

    contract.apply(code, action);
    eosio_exit(0);
}

}