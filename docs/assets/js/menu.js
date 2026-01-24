/**
 * Nevaarize Documentation - Mobile Menu Handler
 * Handles responsive sidebar navigation
 */

(function() {
    'use strict';

    /**
     * Initialize mobile menu functionality
     */
    function initMobileMenu() {
        // Create menu toggle button
        const menuToggle = document.createElement('button');
        menuToggle.className = 'menu-toggle';
        menuToggle.setAttribute('aria-label', 'Toggle navigation menu');
        menuToggle.innerHTML = '<span class="menu-toggle-icon"></span> Menu';
        
        // Create overlay
        const overlay = document.createElement('div');
        overlay.className = 'sidebar-overlay';
        
        // Get sidebar
        const sidebar = document.querySelector('.sidebar');
        const body = document.body;
        
        if (!sidebar) return;
        
        // Insert elements into DOM
        body.insertBefore(menuToggle, body.firstChild);
        body.insertBefore(overlay, sidebar);
        
        // Toggle menu on button click
        menuToggle.addEventListener('click', function(e) {
            e.stopPropagation();
            toggleMenu();
        });
        
        // Close menu on overlay click
        overlay.addEventListener('click', function() {
            closeMenu();
        });
        
        // Close menu when clicking a nav link
        sidebar.querySelectorAll('.nav-link').forEach(function(link) {
            link.addEventListener('click', function() {
                // Small delay to allow navigation
                setTimeout(closeMenu, 100);
            });
        });
        
        // Close menu on escape key
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape' && sidebar.classList.contains('open')) {
                closeMenu();
            }
        });
        
        // Handle window resize
        let resizeTimer;
        window.addEventListener('resize', function() {
            clearTimeout(resizeTimer);
            resizeTimer = setTimeout(function() {
                if (window.innerWidth > 1024) {
                    closeMenu();
                }
            }, 150);
        });
        
        /**
         * Toggle menu open/close
         */
        function toggleMenu() {
            const isOpen = sidebar.classList.contains('open');
            if (isOpen) {
                closeMenu();
            } else {
                openMenu();
            }
        }
        
        /**
         * Open menu
         */
        function openMenu() {
            sidebar.classList.add('open');
            overlay.classList.add('active');
            menuToggle.innerHTML = '<span class="menu-toggle-icon"></span> Close';
            body.style.overflow = 'hidden';
        }
        
        /**
         * Close menu
         */
        function closeMenu() {
            sidebar.classList.remove('open');
            overlay.classList.remove('active');
            menuToggle.innerHTML = '<span class="menu-toggle-icon"></span> Menu';
            body.style.overflow = '';
        }
    }

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initMobileMenu);
    } else {
        initMobileMenu();
    }

})();
