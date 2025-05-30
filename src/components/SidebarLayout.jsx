import React from 'react';
import Sidebar from './Sidebar.jsx';

export default function SidebarLayout({ children }) {
  return (
    <div className="layout">
      <main className="main">
        {children}
      </main>
      <Sidebar />
    </div>
  );
}
