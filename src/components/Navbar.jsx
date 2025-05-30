import React from 'react';
import { NavLink } from 'react-router-dom';

export default function Navbar() {
  return (
    <nav className="navbar">
      <NavLink to="/" end>
        Home
      </NavLink>
      <NavLink to="/about">
        About
      </NavLink>
      <NavLink to="/development">
        Development
      </NavLink>
    </nav>
  );
}
