/*
 * Copyright 2001-2009 Sun Microsystems, Inc.  All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 */

// Forward declarations
class CompactibleFreeListSpace;

class PromotedObject VALUE_OBJ_CLASS_SPEC {
 private:
  enum {
    promoted_mask  = right_n_bits(2),   // i.e. 0x3
    displaced_mark = nth_bit(2),        // i.e. 0x4
    next_mask      = ~(right_n_bits(3)) // i.e. ~(0x7)
  };
  intptr_t _next;
 public:
  inline PromotedObject* next() const {
    return (PromotedObject*)(_next & next_mask);
  }
  inline void setNext(PromotedObject* x) {
    assert(((intptr_t)x & ~next_mask) == 0,
           "Conflict in bit usage, "
           " or insufficient alignment of objects");
    _next |= (intptr_t)x;
  }
  inline void setPromotedMark() {
    _next |= promoted_mask;
  }
  inline bool hasPromotedMark() const {
    return (_next & promoted_mask) == promoted_mask;
  }
  inline void setDisplacedMark() {
    _next |= displaced_mark;
  }
  inline bool hasDisplacedMark() const {
    return (_next & displaced_mark) != 0;
  }
  inline void clearNext()        { _next = 0; }
  debug_only(void *next_addr() { return (void *) &_next; })
};

class SpoolBlock: public FreeChunk {
  friend class PromotionInfo;
 protected:
  SpoolBlock*  nextSpoolBlock;
  size_t       bufferSize;        // number of usable words in this block
  markOop*     displacedHdr;      // the displaced headers start here

  // Note about bufferSize: it denotes the number of entries available plus 1;
  // legal indices range from 1 through BufferSize - 1.  See the verification
  // code verify() that counts the number of displaced headers spooled.
  size_t computeBufferSize() {
    return (size() * sizeof(HeapWord) - sizeof(*this)) / sizeof(markOop);
  }

 public:
  void init() {
    bufferSize = computeBufferSize();
    displacedHdr = (markOop*)&displacedHdr;
    nextSpoolBlock = NULL;
  }

  void print_on(outputStream* st) const;
  void print() const { print_on(gclog_or_tty); }
};

class PromotionInfo VALUE_OBJ_CLASS_SPEC {
  bool            _tracking;      // set if tracking
  CompactibleFreeListSpace* _space; // the space to which this belongs
  PromotedObject* _promoHead;     // head of list of promoted objects
  PromotedObject* _promoTail;     // tail of list of promoted objects
  SpoolBlock*     _spoolHead;     // first spooling block
  SpoolBlock*     _spoolTail;     // last  non-full spooling block or null
  SpoolBlock*     _splice_point;  // when _spoolTail is null, holds list tail
  SpoolBlock*     _spareSpool;    // free spool buffer
  size_t          _firstIndex;    // first active index in
                                  // first spooling block (_spoolHead)
  size_t          _nextIndex;     // last active index + 1 in last
                                  // spooling block (_spoolTail)
 private:
  // ensure that spooling space exists; return true if there is spooling space
  bool ensure_spooling_space_work();

 public:
  PromotionInfo() :
    _tracking(0), _space(NULL),
    _promoHead(NULL), _promoTail(NULL),
    _spoolHead(NULL), _spoolTail(NULL),
    _spareSpool(NULL), _firstIndex(1),
    _nextIndex(1) {}

  bool noPromotions() const {
    assert(_promoHead != NULL || _promoTail == NULL, "list inconsistency");
    return _promoHead == NULL;
  }
  void startTrackingPromotions();
  void stopTrackingPromotions(uint worker_id = 0);
  bool tracking() const          { return _tracking;  }
  void track(PromotedObject* trackOop);      // keep track of a promoted oop
  // The following variant must be used when trackOop is not fully
  // initialized and has a NULL klass:
  void track(PromotedObject* trackOop, klassOop klassOfOop); // keep track of a promoted oop
  void setSpace(CompactibleFreeListSpace* sp) { _space = sp; }
  CompactibleFreeListSpace* space() const     { return _space; }
  markOop nextDisplacedHeader(); // get next header & forward spool pointer
  void    saveDisplacedHeader(markOop hdr);
                                 // save header and forward spool

  inline size_t refillSize() const;

  SpoolBlock* getSpoolBlock();   // return a free spooling block
  inline bool has_spooling_space() {
    return _spoolTail != NULL && _spoolTail->bufferSize > _nextIndex;
  }
  // ensure that spooling space exists
  bool ensure_spooling_space() {
    return has_spooling_space() || ensure_spooling_space_work();
  }
  #define PROMOTED_OOPS_ITERATE_DECL(OopClosureType, nv_suffix)  \
    void promoted_oops_iterate##nv_suffix(OopClosureType* cl);
  ALL_SINCE_SAVE_MARKS_CLOSURES(PROMOTED_OOPS_ITERATE_DECL)
  #undef PROMOTED_OOPS_ITERATE_DECL
  void promoted_oops_iterate(OopsInGenClosure* cl) {
    promoted_oops_iterate_v(cl);
  }
  void verify()  const;
  void reset() {
    _promoHead = NULL;
    _promoTail = NULL;
    _spoolHead = NULL;
    _spoolTail = NULL;
    _spareSpool = NULL;
    _firstIndex = 0;
    _nextIndex = 0;

  }

  void print_on(outputStream* st) const;
  void print_statistics(uint worker_id) const;
};

