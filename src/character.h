#ifndef CHARACTER_H
#define CHARACTER_H

#include "creature.h"
#include "action.h"
#include "inventory.h"
#include "bionics.h"

#include <map>

enum vision_modes {
    DEBUG_NIGHTVISION,
    NV_GOGGLES,
    NIGHTVISION_1,
    NIGHTVISION_2,
    NIGHTVISION_3,
    FULL_ELFA_VISION,
    ELFA_VISION,
    CEPH_VISION,
    FELINE_VISION,
    BIRD_EYE,
    URSINE_VISION,
    NUM_VISION_MODES
};

class Character : public Creature
{
    public:
        virtual ~Character() override { };

        field_id bloodType() const override;
        field_id gibType() const override;
        virtual bool is_warm() const override;
        virtual const std::string &symbol() const override;

        /** Processes effects which may prevent the Character from moving (bear traps, crushed, etc.).
         *  Returns false if movement is stopped. */
        virtual bool move_effects(bool attacking) override;
        /** Performs any Character-specific modifications to the arguments before passing to Creature::add_effect(). */
        virtual void add_effect( efftype_id eff_id, int dur, body_part bp = num_bp, bool permanent = false,
                                 int intensity = 0, bool force = false ) override;

        /** Recalculates HP after a change to max strength */
        void recalc_hp();
        /** Modifies the player's sight values
         *  Must be called when any of the following change:
         *  This must be called when any of the following change:
         * - effects
         * - bionics
         * - traits
         * - underwater
         * - clothes
         */
        void recalc_sight_limits();
        /**
         * Returns the apparent light level at which the player can see.
         * This is adjusted by the light level at the *character's* position
         * to simulate glare, etc, night vision only works if you are in the dark.
         */
        float get_vision_threshold(int light_level) const;
        // --------------- Mutation Stuff ---------------
        // In newcharacter.cpp
        /** Returns the id of a random starting trait that costs >= 0 points */
        std::string random_good_trait();
        /** Returns the id of a random starting trait that costs < 0 points */
        std::string random_bad_trait();

        // In mutation.cpp
        /** Returns true if the player has the entered trait */
        virtual bool has_trait(const std::string &flag) const override;
        /** Returns true if the player has the entered starting trait */
        bool has_base_trait(const std::string &flag) const;
        /** Returns the trait id with the given invlet, or an empty string if no trait has that invlet */
        std::string trait_by_invlet( char ch ) const;

        /** Toggles a trait on the player and in their mutation list */
        void toggle_trait(const std::string &flag);
        /** Toggles a mutation on the player, but does not trigger mutation loss/gain effects. */
        void toggle_mutation(const std::string &flag);

 private:
        /** Retrieves a stat mod of a mutation. */
        int get_mod(std::string mut, std::string arg) const;
 protected:
        /** Applies stat mods to character. */
        void apply_mods(const std::string &mut, bool add_remove);
 public:
        /** Handles things like destruction of armor, etc. */
        void mutation_effect(std::string mut);
        /** Handles what happens when you lose a mutation. */
        void mutation_loss_effect(std::string mut);

        bool has_active_mutation(const std::string &b) const;

        // --------------- Bionic Stuff ---------------
        /** Returns true if the player has the entered bionic id */
        bool has_bionic(const std::string &b) const;
        /** Returns true if the player has the entered bionic id and it is powered on */
        bool has_active_bionic(const std::string &b) const;

        // --------------- Generic Item Stuff ---------------

        struct has_mission_item_filter {
            int mission_id;
            bool operator()(const item &it) {
                return it.mission_id == mission_id;
            }
        };

        // -2 position is 0 worn index, -3 position is 1 worn index, etc
        static int worn_position_to_index(int position)
        {
            return -2 - position;
        }

        // checks to see if an item is worn
        bool is_worn(const item &thing) const
        {
            for(const auto &elem : worn) {
                if(&thing == &elem) {
                    return true;
                }
            }
            return false;
        }

        /**
         * Test whether an item in the possession of this player match a
         * certain filter.
         * The items might be inside other items (containers / quiver / etc.),
         * the filter is recursively applied to all item contents.
         * If this returns true, the vector returned by @ref items_with
         * (with the same filter) will be non-empty.
         * @param filter some object that when invoked with the () operator
         * returns true for item that should checked for.
         * @return Returns true when at least one item matches the filter,
         * if no item matches the filter it returns false.
         */
        template<typename T>
        bool has_item_with(T filter) const
        {
            if( inv.has_item_with( filter ) ) {
                return true;
            }
            if( !weapon.is_null() && inventory::has_item_with_recursive( weapon, filter ) ) {
                return true;
            }
            for( auto &w : worn ) {
                if( inventory::has_item_with_recursive( w, filter ) ) {
                    return true;
                }
            }
            return false;
        }
        /**
         * Gather all items that match a certain filter.
         * The returned vector contains pointers to items in the possession
         * of this player (can be weapon, worn items or inventory).
         * The items might be inside other items (containers / quiver / etc.),
         * the filter is recursively applied to all item contents.
         * The items should not be changed directly, the pointers can be used
         * with @ref i_rem, @ref reduce_charges. The pointers are *not* suitable
         * for @ref get_item_position because the returned index can only
         * refer to items directly in the inventory (e.g. -1 means the weapon,
         * there is no index for the content of the weapon).
         * @param filter some object that when invoked with the () operator
         * returns true for item that should be returned.
         */
        template<typename T>
        std::vector<const item *> items_with(T filter) const
        {
            auto result = inv.items_with( filter );
            if( !weapon.is_null() ) {
                inventory::items_with_recursive( result, weapon, filter );
            }
            for( auto &w : worn ) {
                inventory::items_with_recursive( result, w, filter );
            }
            return result;
        }

        template<typename T>
        std::vector<item *> items_with(T filter)
        {
            auto result = inv.items_with( filter );
            if( !weapon.is_null() ) {
                inventory::items_with_recursive( result, weapon, filter );
            }
            for( auto &w : worn ) {
                inventory::items_with_recursive( result, w, filter );
            }
            return result;
        }
        /**
         * Removes the items that match the given filter.
         * The returned items are a copy of the removed item.
         * If no item has been removed, an empty list will be returned.
         */
        template<typename T>
        std::list<item> remove_items_with( T filter )
        {
            // player usually interacts with items in the inventory the most (?)
            std::list<item> result = inv.remove_items_with( filter );
            for( auto iter = worn.begin(); iter != worn.end(); ) {
                item &article = *iter;
                if( filter( article ) ) {
                    result.splice( result.begin(), worn, iter++ );
                } else {
                    result.splice( result.begin(), article.remove_items_with( filter ) );
                    ++iter;
                }
            }
            if( !weapon.is_null() ) {
                if( filter( weapon ) ) {
                    result.push_back( remove_weapon() );
                } else {
                    result.splice( result.begin(), weapon.remove_items_with( filter ) );
                }
            }
            return result;
        }
        /**
         * Similar to @ref remove_items_with, but considers only worn items and not their
         * content (@ref item::contents is not checked).
         * If the filter function returns true, the item is removed.
         */
        std::list<item> remove_worn_items_with( std::function<bool(item &)> filter );

        item &i_add(item it);
        /**
         * Remove a specific item from player possession. The item is compared
         * by pointer. Contents of the item are removed as well.
         * @param pos The item position of the item to be removed. The item *must*
         * exists, use @ref has_item to check this.
         * @return A copy of the removed item.
         */
        item i_rem(int pos);
        /**
         * Remove a specific item from player possession. The item is compared
         * by pointer. Contents of the item are removed as well.
         * @param it A pointer to the item to be removed. The item *must* exists
         * in the players possession (one can use @ref has_item to check for this).
         * @return A copy of the removed item.
         */
        item i_rem(const item *it);
        void i_rem_keep_contents( int pos );
        /** Sets invlet and adds to inventory if possible, drops otherwise, returns true if either succeeded.
         *  An optional qty can be provided (and will perform better than separate calls). */
        bool i_add_or_drop(item &it, int qty = 1);

        /** Only use for UI things. Returns all invlets that are currently used in
         * the player inventory, the weapon slot and the worn items. */
        std::set<char> allocated_invlets() const;

        /**
         * Whether the player carries an active item of the given item type.
         */
        bool has_active_item(const itype_id &id) const;
        item remove_weapon();
        void remove_mission_items(int mission_id);

        int weight_carried() const;
        int volume_carried() const;
        int weight_capacity() const override;
        int volume_capacity() const;
        bool can_pickVolume(int volume, bool safe = false) const;
        bool can_pickWeight(int weight, bool safe = true) const;

        bool has_artifact_with(const art_effect_passive effect) const;

        // --------------- Clothing Stuff ---------------
        /** Returns true if the player is wearing the item. */
        bool is_wearing(const itype_id &it) const;
        /** Returns true if the player is wearing the item on the given body_part. */
        bool is_wearing_on_bp(const itype_id &it, body_part bp) const;
        /** Returns true if the player is wearing an item with the given flag. */
        bool worn_with_flag( std::string flag ) const;

        // --------------- Skill Stuff ---------------
        SkillLevel &skillLevel(const Skill* _skill);
        SkillLevel &skillLevel(Skill const &_skill);
        SkillLevel &skillLevel(std::string ident);

        /** for serialization */
        SkillLevel const& get_skill_level(const Skill* _skill) const;
        SkillLevel const& get_skill_level(const Skill &_skill) const;
        SkillLevel const& get_skill_level(const std::string &ident) const;

        // --------------- Other Stuff ---------------


        /** return the calendar::turn the character expired */
        int get_turn_died() const
        {
            return turn_died;
        }
        /** set the turn the turn the character died if not already done */
        void set_turn_died(int turn)
        {
            turn_died = (turn_died != -1) ? turn : turn_died;
        }

        /** Calls Creature::normalize()
         *  nulls out the player's weapon
         *  Should only be called through player::normalize(), not on it's own!
         */
        virtual void normalize() override;
        virtual void die(Creature *nkiller) override;

        /** Resets stats, and applies effects in an idempotent manner */
        virtual void reset_stats() override;

        /** Returns true if the player has some form of night vision */
        bool has_nv();

        // In newcharacter.cpp
        void empty_skills();
        /** Returns a random name from NAMES_* */
        void pick_name();
        /** Get the idents of all base traits. */
        std::vector<std::string> get_base_traits() const;
        /** Get the idents of all traits/mutations. */
        std::vector<std::string> get_mutations() const;
        /** Empties the trait list */
        void empty_traits();
        void add_traits();

        // --------------- Values ---------------
        std::string name;
        bool male;

        std::list<item> worn;
        std::array<int, num_hp_parts> hp_cur, hp_max;
        bool nv_cached;

        inventory inv;
        std::map<char, itype_id> assigned_invlet;
        itype_id last_item;
        item weapon;
        item ret_null; // Null item, sometimes returns by weapon() etc

        std::vector<bionic> my_bionics;

    protected:
        Character();
        Character(const Character &) = default;
        Character(Character &&) = default;
        Character &operator=(const Character &) = default;
        Character &operator=(Character &&) = default;
        struct trait_data : public JsonSerializer, public JsonDeserializer {
            /** Key to select the mutation in the UI. */
            char key = ' ';
            /**
             * Time (in turns) until the mutation increase hunger/thirst/fatigue according
             * to its cost (@ref mutation_data::cost). When those costs have been paid, this
             * is reset to @ref mutation_data::cooldown.
             */
            int charge = 0;
            /** Whether the mutation is activated. */
            bool powered = false;
            // -- serialization stuff, see savegame_json.cpp
            using JsonSerializer::serialize;
            void serialize( JsonOut &json ) const override;
            using JsonDeserializer::deserialize;
            void deserialize( JsonIn &jsin ) override;
        };
        /**
         * Traits / mutations of the character. Key is the mutation id (it's also a valid
         * key into @ref mutation_data), the value describes the status of the mutation.
         * If there is not entry for a mutation, the character does not have it. If the map
         * contains the entry, the character has the mutation.
         */
        std::unordered_map<std::string, trait_data> my_mutations;
        /**
         * Contains mutation ids of the base traits.
         */
        std::unordered_set<std::string> my_traits;

        void store(JsonOut &jsout) const;
        void load(JsonObject &jsin);

        // --------------- Values ---------------
        std::map<const Skill*, SkillLevel> _skills;

        // Cached vision values.
        std::bitset<NUM_VISION_MODES> vision_mode_cache;
        int sight_max;

        // turn the character expired, if -1 it has not been set yet.
        int turn_died = -1;
};

#endif
