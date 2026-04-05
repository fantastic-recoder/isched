import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { App } from './app';
import { BootstrapService } from './services/bootstrap.service';

describe('App', () => {
  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [App],
      providers: [
        {
          provide: BootstrapService,
          useValue: {
            bootstrapStatus: () => of({ systemState: { seedModeActive: false } }),
          },
        },
      ],
    }).compileComponents();
  });

  it('should create the app', () => {
    const fixture = TestBed.createComponent(App);
    const app = fixture.componentInstance;
    expect(app).toBeTruthy();
  });

  it('should not show bootstrap banner when seed mode is inactive', async () => {
    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();
    await fixture.whenStable();

    const compiled = fixture.nativeElement as HTMLElement;
    expect(compiled.textContent).not.toContain('Bootstrap mode active');
  });
});
