/**
 * This script adds simple search/fitler functionality to the html version
 * of the lensfun coverage table.
 *
 * Example queries:
 * `Canon` => Only shows Canon lenses
 * `Canon !-` => Shows prime Canon lenses
 * `Canon !- is` => Shows prime Canon lenses with Image Stabilization
 */

document.addEventListener('DOMContentLoaded', function() {

    function fixupMakers(visibleRows) {
        visibleRows.forEach((row, index) => {
            const current = row.getAttribute('data-maker');
            const prev = index > 0 ? visibleRows[index - 1].getAttribute('data-maker') : '';

            // Hide sequential maker names if they are the same
            if (current === prev) {
                row.children[0].textContent = "";
            } else {
                row.children[0].textContent = current;
            }
        });
    }

    const rows = document.querySelectorAll('#searchableTable tbody tr');
    const searchBox = document.getElementById('searchbox');
    const clearButton = document.getElementById('searchclear');
    searchBox.value = "";

    // The main search box functionality
    searchBox.addEventListener('input', function() {
        // Split apart into valid search terms
        const searchTerms = this.value.toLowerCase().split(' ').filter(term => term.length > 0 && (term.length > 1 || term[0] != '!'));
        clearButton.disabled = (searchTerms.length > 0) ? "" : "disabled";

        let visibleRows = [];
        rows.forEach(row => {
            const maker = row.getAttribute('data-maker').toLowerCase();
            const model = row.getAttribute('data-model').toLowerCase();

            let shouldShow = true; // if no search terms, show everything

            if (searchTerms.length > 0) {
                // A row must match ALL search terms
                shouldShow = searchTerms.every(term => {
                    if (term[0] == '!') {
                        term = term.slice(1);
                        return !(maker.includes(term) || model.includes(term));
                    } else {
                        return maker.includes(term) || model.includes(term)
                    }
                });
            }

            // directly update the visibility of each row that we are filtering
            row.style.display = shouldShow ? '' : 'none';
            if (shouldShow)
                visibleRows.push(row);
        });

        fixupMakers(visibleRows); // rows may be missing, show the maker in the first visble rows
    });

    // The clear button will reset all searches
    clearButton.addEventListener('click', function() {
        searchBox.value = "";
        searchBox.dispatchEvent(new Event('input', { bubbles: true }));
    });
});
