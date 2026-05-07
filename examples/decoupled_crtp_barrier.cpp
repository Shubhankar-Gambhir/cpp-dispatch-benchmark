// Approach 3: Decoupled CRTP + Lazy Resolution + Compile-Time AOP
//
// Modeled on OpenJDK's BarrierSet / AccessBarrier / RuntimeDispatch.
//
// Three patterns working together:
//   1. Decoupled CRTP  — non-template base, static singleton, FakeRtti
//   2. Compile-time AOP — AccessBarrier inheritance chain layers aspects
//   3. Lazy resolution  — resolve once, patch function pointer, one indirect call thereafter
//
// The dispatch is still an indirect call through a function pointer — the
// compiler cannot inline across it. The win is what's *behind* the pointer:
// the AccessBarrier template chain flattens pre-barrier + raw store +
// post-barrier into straight-line code at compile time. Compare with virtual
// dispatch which adds a second indirection (vptr → vtable → function) and
// requires duplicating barrier logic across GCs instead of composing it.
//
// The base class (BarrierSet) compiles without knowing any concrete GC type.
// The CRTP connection only exists in the nested AccessBarrier template and
// the GetName/GetType specializations each concrete GC provides.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

// ============================================================
// FakeRttiSupport — bitset-based type checking without C++ RTTI
// (cf. hotspot/share/utilities/fakeRttiSupport.hpp)
// ============================================================

template <typename T, typename TagType>
class FakeRttiSupport {
public:
    explicit FakeRttiSupport(TagType concrete_tag)
        : tag_set_(tag_bit(concrete_tag)), concrete_tag_(concrete_tag) {}

    FakeRttiSupport(TagType concrete_tag, uint32_t tag_set)
        : tag_set_(tag_set), concrete_tag_(concrete_tag) {}

    TagType concrete_tag() const { return concrete_tag_; }

    bool has_tag(TagType tag) const {
        return (tag_set_ & tag_bit(tag)) != 0;
    }

    FakeRttiSupport add_tag(TagType tag) const {
        return FakeRttiSupport(concrete_tag_, tag_set_ | tag_bit(tag));
    }

private:
    uint32_t tag_set_;
    TagType concrete_tag_;

    static uint32_t tag_bit(TagType tag) {
        return 1u << static_cast<unsigned>(tag);
    }
};

// ============================================================
// BarrierSet — non-template base class
// (cf. hotspot/share/gc/shared/barrierSet.hpp)
// ============================================================

class BarrierSet {
public:
    enum Name { EpsilonBS, SerialBS, G1BS, ModRefBS, UnknownBS };

protected:
    using FakeRtti = FakeRttiSupport<BarrierSet, Name>;

private:
    FakeRtti fake_rtti_;
    static BarrierSet* _barrier_set;

public:
    explicit BarrierSet(FakeRtti fake_rtti) : fake_rtti_(fake_rtti) {}
    virtual ~BarrierSet() = default;

    Name kind() const { return fake_rtti_.concrete_tag(); }
    bool is_a(Name n) const { return fake_rtti_.has_tag(n); }

    static BarrierSet* barrier_set() { return _barrier_set; }
    static void set_barrier_set(BarrierSet* bs) { _barrier_set = bs; }

    // --- Metafunctions: type <-> enum ---
    template <typename T> struct GetName;
    template <Name N> struct GetType;

    // --- AccessBarrier: ALL STATIC methods, no `this` ---
    // Layer 0: raw write, no barriers
    template <typename BarrierSetT>
    class AccessBarrier {
    public:
        static void store(int* addr, int value) {
            std::printf("  [raw store] *addr = %d\n", value);
            *addr = value;
        }

        static void store_at(int* base, int offset, int value) {
            store(base + offset, value);
        }
    };
};

BarrierSet* BarrierSet::_barrier_set = nullptr;

// --- Checked downcast (cf. barrier_set_cast) ---

template <typename T>
T* barrier_set_cast(BarrierSet* bs) {
    assert(bs->is_a(BarrierSet::GetName<T>::value));
    return static_cast<T*>(bs);
}

// ============================================================
// ModRefBarrierSet — intermediate layer, adds post-barrier aspect
// (cf. hotspot/share/gc/shared/modRefBarrierSet.hpp)
// ============================================================

class ModRefBarrierSet : public BarrierSet {
public:
    void write_ref_field_post(int* addr) {
        std::printf("  [post-barrier] card mark @ %p\n", static_cast<void*>(addr));
    }

protected:
    ModRefBarrierSet(FakeRtti fake_rtti)
        : BarrierSet(fake_rtti.add_tag(BarrierSet::ModRefBS)) {}

public:
    // Layer 1: raw write + post-barrier (card marking)
    template <typename BarrierSetT>
    class AccessBarrier : public BarrierSet::AccessBarrier<BarrierSetT> {
        using Raw = BarrierSet::AccessBarrier<BarrierSetT>;
    public:
        static void store(int* addr, int value) {
            auto* bs = barrier_set_cast<BarrierSetT>(BarrierSet::barrier_set());
            Raw::store(addr, value);
            bs->write_ref_field_post(addr);
        }

        static void store_at(int* base, int offset, int value) {
            store(base + offset, value);
        }
    };
};

// ============================================================
// EpsilonBarrierSet — no barriers (cf. gc/epsilon)
// ============================================================

class EpsilonBarrierSet : public BarrierSet {
public:
    EpsilonBarrierSet() : BarrierSet(FakeRtti(BarrierSet::EpsilonBS)) {}

    // Inherits base AccessBarrier unchanged — raw write, no barriers
    template <typename BarrierSetT = EpsilonBarrierSet>
    class AccessBarrier : public BarrierSet::AccessBarrier<BarrierSetT> {};
};

template <> struct BarrierSet::GetName<EpsilonBarrierSet> {
    static constexpr Name value = BarrierSet::EpsilonBS;
};
template <> struct BarrierSet::GetType<BarrierSet::EpsilonBS> {
    using type = EpsilonBarrierSet;
};

// ============================================================
// SerialBarrierSet — post-barrier only (cf. gc/serial)
// ============================================================

class SerialBarrierSet : public ModRefBarrierSet {
public:
    SerialBarrierSet() : ModRefBarrierSet(FakeRtti(BarrierSet::SerialBS)) {}

    // Inherits ModRef AccessBarrier — gets card marking for free
    template <typename BarrierSetT = SerialBarrierSet>
    class AccessBarrier : public ModRefBarrierSet::AccessBarrier<BarrierSetT> {};
};

template <> struct BarrierSet::GetName<SerialBarrierSet> {
    static constexpr Name value = BarrierSet::SerialBS;
};
template <> struct BarrierSet::GetType<BarrierSet::SerialBS> {
    using type = SerialBarrierSet;
};

// ============================================================
// G1BarrierSet — pre-barrier (SATB) + post-barrier (card marking)
// (cf. hotspot/share/gc/g1/g1BarrierSet.hpp)
// ============================================================

class G1BarrierSet : public ModRefBarrierSet {
public:
    G1BarrierSet() : ModRefBarrierSet(FakeRtti(BarrierSet::G1BS)) {}

    void write_ref_field_pre(int* addr) {
        std::printf("  [pre-barrier] SATB snapshot: old=%d\n", *addr);
    }

    // Layer 2: adds SATB pre-barrier, delegates to ModRef for raw + post
    template <typename BarrierSetT = G1BarrierSet>
    class AccessBarrier : public ModRefBarrierSet::AccessBarrier<BarrierSetT> {
        using ModRef = ModRefBarrierSet::AccessBarrier<BarrierSetT>;
    public:
        static void store(int* addr, int value) {
            auto* bs = barrier_set_cast<G1BarrierSet>(BarrierSet::barrier_set());
            bs->write_ref_field_pre(addr);
            ModRef::store(addr, value);
        }

        static void store_at(int* base, int offset, int value) {
            store(base + offset, value);
        }
    };
};

template <> struct BarrierSet::GetName<G1BarrierSet> {
    static constexpr Name value = BarrierSet::G1BS;
};
template <> struct BarrierSet::GetType<BarrierSet::G1BS> {
    using type = G1BarrierSet;
};

// ============================================================
// RuntimeDispatch — lazy resolution, resolve once, direct calls forever
// (cf. hotspot/share/oops/accessBackend.hpp RuntimeDispatch)
// ============================================================

struct RuntimeDispatch {
    using StoreFn   = void(*)(int*, int);
    using StoreAtFn = void(*)(int*, int, int);

    static StoreFn   _store_func;
    static StoreAtFn _store_at_func;

    // Resolvers — run ONCE on first call, then patch the pointer
    static void store_init(int* addr, int value) {
        std::printf("  [resolver] first call — resolving barrier set (kind=%d)\n",
                    BarrierSet::barrier_set()->kind());
        StoreFn func = resolve_store();
        _store_func = func;
        func(addr, value);
    }

    static void store_at_init(int* base, int offset, int value) {
        std::printf("  [resolver] first store_at — resolving barrier set (kind=%d)\n",
                    BarrierSet::barrier_set()->kind());
        StoreAtFn func = resolve_store_at();
        _store_at_func = func;
        func(base, offset, value);
    }

    // Public API — after first call, single indirect call (no vtable double-indirection)
    static void store(int* addr, int value) {
        _store_func(addr, value);
    }

    static void store_at(int* base, int offset, int value) {
        _store_at_func(base, offset, value);
    }

private:
    // Switch on FakeRtti enum — maps runtime kind() to compile-time AccessBarrier
    // (cf. access.inline.hpp BarrierResolver::resolve_barrier_gc)
    static StoreFn resolve_store() {
        switch (BarrierSet::barrier_set()->kind()) {
            case BarrierSet::EpsilonBS:
                return &EpsilonBarrierSet::AccessBarrier<>::store;
            case BarrierSet::SerialBS:
                return &SerialBarrierSet::AccessBarrier<>::store;
            case BarrierSet::G1BS:
                return &G1BarrierSet::AccessBarrier<>::store;
            default:
                throw std::runtime_error("unknown barrier set");
        }
    }

    static StoreAtFn resolve_store_at() {
        switch (BarrierSet::barrier_set()->kind()) {
            case BarrierSet::EpsilonBS:
                return &EpsilonBarrierSet::AccessBarrier<>::store_at;
            case BarrierSet::SerialBS:
                return &SerialBarrierSet::AccessBarrier<>::store_at;
            case BarrierSet::G1BS:
                return &G1BarrierSet::AccessBarrier<>::store_at;
            default:
                throw std::runtime_error("unknown barrier set");
        }
    }
};

// Function pointers start pointing at the resolver
RuntimeDispatch::StoreFn   RuntimeDispatch::_store_func   = &RuntimeDispatch::store_init;
RuntimeDispatch::StoreAtFn RuntimeDispatch::_store_at_func = &RuntimeDispatch::store_at_init;

// ============================================================
// HeapAccess — public API (cf. oops/access.hpp HeapAccess)
// ============================================================

struct HeapAccess {
    static void store(int* addr, int value) {
        RuntimeDispatch::store(addr, value);
    }
    static void store_at(int* base, int offset, int value) {
        RuntimeDispatch::store_at(base, offset, value);
    }
};

// ============================================================
// Demo
// ============================================================

static BarrierSet* create_barrier_set(const char* gc_name) {
    if (std::strcmp(gc_name, "epsilon") == 0) return new EpsilonBarrierSet();
    if (std::strcmp(gc_name, "serial") == 0)  return new SerialBarrierSet();
    if (std::strcmp(gc_name, "g1") == 0)      return new G1BarrierSet();
    throw std::runtime_error(std::string("unknown GC: ") + gc_name);
}

int main(int argc, char* argv[]) {
    const char* gc = (argc > 1) ? argv[1] : "g1";

    // Runtime selection — like JVM startup with -XX:+UseG1GC
    BarrierSet* bs = create_barrier_set(gc);
    BarrierSet::set_barrier_set(bs);

    std::printf("=== Decoupled CRTP + Lazy Resolution — %s barriers ===\n", gc);
    std::printf("kind() = %d, is_a(G1BS) = %d, is_a(ModRefBS) = %d\n\n",
                bs->kind(), bs->is_a(BarrierSet::G1BS), bs->is_a(BarrierSet::ModRefBS));

    int heap[8] = {};

    for (int i = 0; i < 3; ++i) {
        std::printf("store_at(heap, %d, %d):\n", i, (i + 1) * 42);
        HeapAccess::store_at(heap, i, (i + 1) * 42);
    }

    std::printf("\nheap: [");
    for (int i = 0; i < 8; ++i) std::printf("%s%d", i ? ", " : "", heap[i]);
    std::printf("]\n");

    delete bs;
}
