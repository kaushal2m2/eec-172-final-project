import React from 'react';
import '../styles/About.css';

export default function About() {
  return (
    <div className="about-container">
      <div className="about-header">
        <div className="header-content">
          <h1 className="about-title">TI-OS™ Features</h1>
          <p className="about-intro">
            TI-OS™ offers a powerful platform for embedded system development, 
            with advanced features and capabilities designed for the Texas Instruments ecosystem.
          </p>
        </div>
      </div>

      <div className="card-grid">
        {/* Card 1: Texas icon */}
        <div className="card">
          <div className="card-icon">
            <img src="assets/img1.png" alt="Texas Icon" className="card-image" />
          </div>
          <div className="card-content">
            <h3 className="card-title">Texas Hardware Support</h3>
            <p className="card-text">
              Full support for Texas Instruments hardware platforms and microcontrollers with optimized drivers.
            </p>
          </div>
        </div>

        {/* Card 2: Display and Input */}
        <div className="card">
          <div className="card-icon">
            <img src="assets/img2.png" alt="Display Icon" className="card-image" />
          </div>
          <div className="card-content">
            <h3 className="card-title">Display & Input Systems</h3>
            <p className="card-text">
              Configurable interface with support for various input methods and display technologies.
            </p>
          </div>
          
        </div>

        {/* Card 3: Connectivity */}
        <div className="card">
          <div className="card-icon">
            <img src="assets/img3.png" alt="Connectivity Icon" className="card-image" />
          </div>
          <div className="card-content">
            <h3 className="card-title">Connectivity Options</h3>
            <p className="card-text">
              Built-in support for various communication protocols including Wi-Fi, Bluetooth, and ethernet.
            </p>
          </div>
        </div>

        {/* Card 4: Applications */}
        <div className="card">
          <div className="card-icon">
            <img src="assets/img4.png" alt="Applications Icon" className="card-image" />
          </div>
          <div className="card-content">
            <h3 className="card-title">Application Framework</h3>
            <p className="card-text">
              Rich set of APIs and libraries for building robust applications with minimal development time.
            </p>
          </div>
        </div>
      </div>
      
      <div className="about-footer">
        <div className="footer-content">
          <h3>System Requirements</h3>
          <div className="footer-columns">
            <div className="footer-column">
              <h4>Hardware</h4>
              <ul>
                <li>TI MSP430 or ARM Cortex-M series</li>
                <li>Minimum 32KB Flash, 8KB RAM</li>
                <li>GPIO support for peripherals</li>
              </ul>
            </div>
            <div className="footer-column">
              <h4>Development</h4>
              <ul>
                <li>Code Composer Studio v10+</li>
                <li>TI-OS SDK version 2.1 or higher</li>
                <li>Compatible with standard JTAG debuggers</li>
              </ul>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
