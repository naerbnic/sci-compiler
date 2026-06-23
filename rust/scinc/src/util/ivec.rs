use std::{
    mem::MaybeUninit,
    ops::{Deref, DerefMut},
};

pub(crate) struct IVec<const N: usize, T> {
    len: usize,
    data: [MaybeUninit<T>; N],
}

impl<const N: usize, T> IVec<N, T> {
    pub(crate) fn new() -> Self {
        Self {
            len: 0,
            data: MaybeUninit::<[T; N]>::uninit().into(),
        }
    }

    fn as_slice(&self) -> &[T] {
        let inited_slice = &self.data[..self.len];
        let data_ptr = inited_slice.as_ptr();
        let length = inited_slice.len();
        unsafe { std::slice::from_raw_parts(data_ptr as *const T, length) }
    }

    fn as_mut_slice(&mut self) -> &mut [T] {
        let inited_slice = &mut self.data[..self.len];
        let length = inited_slice.len();
        let data_ptr = inited_slice.as_mut_ptr();
        unsafe { std::slice::from_raw_parts_mut(data_ptr as *mut T, length) }
    }

    fn push(&mut self, value: T) -> Result<(), T> {
        if self.len == N {
            return Err(value);
        }
        self.data[self.len].write(value);
        self.len += 1;
        Ok(())
    }

    fn pop(&mut self) -> Option<T> {
        if self.len == 0 {
            None
        } else {
            self.len -= 1;
            Some(unsafe { self.data[self.len].assume_init_read() })
        }
    }

    pub(crate) fn try_insert(&mut self, index: usize, value: T) -> Result<(), T> {
        assert!(index <= self.len, "Index out of bounds");
        if self.len == N {
            return Err(value);
        }
        // To make sure that we are in a consistent state, set the length to
        // the minimum required length before shifting elements.
        let prev_len = self.len;
        self.len = index;
        let copy_len = prev_len - index;

        // As an array, we can do a simple memcopy to shift elements, as long
        // as overlapping is allowed.

        // SAFETY: src_ptr to src_ptr + copy_len is within bounds of the array by assertions above.
        let src_ptr = unsafe { self.data.as_mut_ptr().add(index) };
        // SAFETY: src_ptr to src_ptr + copy_len is within bounds of the array by assertions above.
        let dst_ptr = unsafe { src_ptr.add(1) };

        unsafe { src_ptr.copy_to(dst_ptr, copy_len) };

        // Now data[index] has been copied away, and can be considered uninitialized.
        // By performing write, we have ensured that all items [0..prev_len+1) are properly initialized.
        self.data[index].write(value);

        // As all the elements are initialized, we can now set len to the final value.
        self.len = prev_len + 1;
        Ok(())
    }

    pub(crate) fn insert(&mut self, index: usize, value: T) {
        let Ok(()) = self.try_insert(index, value) else {
            panic!("Failed to insert element at index {}", index)
        };
    }

    pub(crate) fn len(&self) -> usize {
        self.len
    }

    pub(crate) const fn capacity(&self) -> usize {
        N
    }
}

impl<const N: usize, T> Deref for IVec<N, T> {
    type Target = [T];

    fn deref(&self) -> &Self::Target {
        self.as_slice()
    }
}

impl<const N: usize, T> DerefMut for IVec<N, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.as_mut_slice()
    }
}
