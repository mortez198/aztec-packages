use dep::aztec3::notes::value_note::Note;
use dep::aztec3::notes::value_note::NOTE_LEN;
use dep::aztec3::types::point::Point;
use dep::aztec3::utils::arr_copy_slice;

use dep::aztec3::oracle::notes::view_notes_page_internal;
use dep::aztec3::oracle::notes::NOTES_PER_PAGE;

fn view_notes_page(
    storage_slot: Field,
    offset: Field,
) -> (Field, [Note; NOTES_PER_PAGE]){
    let fields = view_notes_page_internal(storage_slot, offset);
    let total_notes = fields[0];
    let mut notes = [Note::dummy(); NOTES_PER_PAGE];
    for i in 0..NOTES_PER_PAGE {
        let read_offset: comptime Field = 1 + i*NOTE_LEN;
        let note = Note::deserialize(arr_copy_slice(fields, [0; NOTE_LEN], read_offset));
        notes[i] = note;
    };
    (total_notes, notes)
}


// TODO(1.5) We don't support yet more than one page of notes
// noir is having issues with loop handling in unconstrained functions
// rewrite to unconstrained and integrate into noir-aztec3 when fixed
fn get_balance(storage_slot: Field) -> Field {
    let mut balance = 0;

    let mut current_batch = view_notes_page(storage_slot, 0).1;
    
    for j in 0..current_batch.len() {
        balance += current_batch[j].value;
    }

    balance
}