use std::{
    borrow::Borrow,
    ops::{Add, Deref, Sub},
    ptr::NonNull,
};

pub enum IntRepr {
    Word([u8; 2]),
    Byte([u8; 1]),
}

impl Deref for IntRepr {
    type Target = [u8];

    fn deref(&self) -> &Self::Target {
        match self {
            IntRepr::Word(bytes) => bytes,
            IntRepr::Byte(bytes) => bytes,
        }
    }
}

/// A 16-bit word that is used in registers and memory.
///
/// This type is neither signed or unsigned inherently, as many operations
/// use it in both modes. It is guaranteed to be a 2s-completement
/// signed value if treated as signed, and a 16-bit unsigned value if
/// treated as unsigned.
///
/// Note that this type does not provide an ordering, as the ordering depends
/// on whether the value is treated as signed or unsigned.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct MWord(u16);

impl MWord {
    pub fn from_u16(value: u16) -> Self {
        MWord(value)
    }

    pub fn from_i16(value: i16) -> Self {
        MWord(value.cast_unsigned())
    }

    pub fn try_to_signed_byte(self) -> Option<MByte> {
        self.to_signed().try_to_byte().map(Into::into)
    }

    pub fn try_to_unsigned_byte(self) -> Option<MByte> {
        self.to_unsigned().try_to_byte().map(Into::into)
    }

    pub fn to_signed(self) -> SWord {
        SWord(self.0.cast_signed())
    }

    pub fn to_unsigned(self) -> UWord {
        UWord(self.0)
    }

    pub fn as_u16(self) -> u16 {
        self.0
    }

    pub fn as_i16(self) -> i16 {
        self.0.cast_signed()
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Word(self.0.to_le_bytes())
    }
}

impl Add for MWord {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        // Addition is normal wrapping u16 addition. This handles 2s-complement
        // signed addition as well as unsigned addition.
        MWord(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for MWord {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        // Subtraction is normal wrapping u16 subtraction. This handles 2s-complement
        // signed subtraction as well as unsigned subtraction.
        MWord(self.0.wrapping_sub(rhs.0))
    }
}

impl From<SWord> for MWord {
    fn from(s: SWord) -> Self {
        s.to_machine()
    }
}

impl From<UWord> for MWord {
    fn from(u: UWord) -> Self {
        MWord(u.0)
    }
}

// Borrow impls for allowing lookups of plain values in maps.

impl Borrow<u16> for MWord {
    fn borrow(&self) -> &u16 {
        &self.0
    }
}

impl Borrow<i16> for MWord {
    fn borrow(&self) -> &i16 {
        unsafe { NonNull::from_ref(&self.0).cast::<i16>().as_ref() }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SWord(i16);

impl SWord {
    pub fn from_i16(value: i16) -> Self {
        SWord(value)
    }

    pub fn try_to_byte(self) -> Option<SByte> {
        i8::try_from(self.0).ok().map(SByte)
    }

    pub fn to_machine(self) -> MWord {
        MWord(self.0.cast_unsigned())
    }

    pub fn as_i16(self) -> i16 {
        self.0
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Word(self.0.to_le_bytes())
    }
}

impl Add for SWord {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for SWord {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_sub(rhs.0))
    }
}

impl From<MWord> for SWord {
    fn from(m: MWord) -> Self {
        m.to_signed()
    }
}

impl From<i16> for SWord {
    fn from(i: i16) -> Self {
        SWord(i)
    }
}

impl Borrow<i16> for SWord {
    fn borrow(&self) -> &i16 {
        &self.0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct UWord(u16);

impl UWord {
    pub fn try_to_byte(self) -> Option<UByte> {
        u8::try_from(self.0).ok().map(UByte)
    }

    pub fn to_machine(self) -> MWord {
        MWord(self.0)
    }

    pub fn as_u16(self) -> u16 {
        self.0
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Word(self.0.to_le_bytes())
    }

    pub fn rel_to(self, other: UWord) -> SWord {
        SWord(self.0.wrapping_sub(other.0).cast_signed())
    }
}

impl Add for UWord {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for UWord {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_sub(rhs.0))
    }
}

impl From<MWord> for UWord {
    fn from(m: MWord) -> Self {
        Self(m.0)
    }
}

impl Borrow<u16> for UWord {
    fn borrow(&self) -> &u16 {
        &self.0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct MByte(u8);

impl MByte {
    pub fn to_signed(self) -> SByte {
        SByte(self.0.cast_signed())
    }

    pub fn to_unsigned(self) -> UByte {
        UByte(self.0)
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Byte([self.0])
    }

    pub fn extend_unsigned(self) -> MWord {
        MWord(self.0.into())
    }

    pub fn extend_signed(self) -> MWord {
        MWord(i16::from(self.0.cast_signed()).cast_unsigned())
    }
}

impl Add for MByte {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        // Addition is normal wrapping u8 addition. This handles 2s-complement
        // signed addition as well as unsigned addition.
        Self(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for MByte {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_sub(rhs.0))
    }
}

impl From<SByte> for MByte {
    fn from(s: SByte) -> Self {
        MByte(s.0.cast_unsigned())
    }
}

impl From<UByte> for MByte {
    fn from(u: UByte) -> Self {
        MByte(u.0)
    }
}

impl Borrow<u8> for MByte {
    fn borrow(&self) -> &u8 {
        &self.0
    }
}

impl Borrow<i8> for MByte {
    fn borrow(&self) -> &i8 {
        unsafe { NonNull::from_ref(&self.0).cast::<i8>().as_ref() }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct SByte(i8);

impl SByte {
    pub fn to_machine(self) -> MByte {
        MByte(self.0.cast_unsigned())
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Byte([self.0.cast_unsigned()])
    }

    pub fn as_i8(self) -> i8 {
        self.0
    }

    pub fn extend(self) -> SWord {
        SWord(self.0.into())
    }
}

impl Add for SByte {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for SByte {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_sub(rhs.0))
    }
}

impl From<MByte> for SByte {
    fn from(m: MByte) -> Self {
        m.to_signed()
    }
}

impl Borrow<i8> for SByte {
    fn borrow(&self) -> &i8 {
        &self.0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct UByte(u8);

impl UByte {
    pub fn to_machine(self) -> MByte {
        MByte(self.0)
    }

    pub fn to_repr(self) -> IntRepr {
        IntRepr::Byte([self.0])
    }

    pub fn as_u8(self) -> u8 {
        self.0
    }

    pub fn extend(self) -> UWord {
        UWord(self.0.into())
    }
}

impl Add for UByte {
    type Output = Self;

    fn add(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_add(rhs.0))
    }
}

impl Sub for UByte {
    type Output = Self;

    fn sub(self, rhs: Self) -> Self::Output {
        Self(self.0.wrapping_sub(rhs.0))
    }
}

impl From<MByte> for UByte {
    fn from(u: MByte) -> Self {
        u.to_unsigned()
    }
}

impl From<u8> for UByte {
    fn from(u: u8) -> Self {
        UByte(u)
    }
}

impl Borrow<u8> for UByte {
    fn borrow(&self) -> &u8 {
        &self.0
    }
}
