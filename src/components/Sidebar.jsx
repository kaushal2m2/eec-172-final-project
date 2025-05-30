import React from 'react';
import sidebarImg from '../assets/sidebar.png';

export default function Sidebar() {
  return (
    <aside className="sidebar">
      <img src={sidebarImg} alt="TI-OS Sidebar" />
    </aside>
  );
}
