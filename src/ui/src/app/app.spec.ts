import { TestBed } from '@angular/core/testing';
import { BehaviorSubject } from 'rxjs';
import { App } from './app';
import { BootstrapService } from './services/bootstrap.service';

describe('App', () => {
  const status$ = new BehaviorSubject({ systemState: { seedModeActive: false } });
  const hint$ = new BehaviorSubject<boolean | null>(null);

  beforeEach(async () => {
    status$.next({ systemState: { seedModeActive: false } });
    hint$.next(null);

    await TestBed.configureTestingModule({
      imports: [App],
      providers: [
        {
          provide: BootstrapService,
          useValue: {
            bootstrapStatus: () => status$.asObservable(),
            seedModeHint$: () => hint$.asObservable(),
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

  it('should switch off bootstrap banner after bootstrap completes', async () => {
    const fixture = TestBed.createComponent(App);

    // Start in seed mode to show the banner.
    status$.next({ systemState: { seedModeActive: true } });
    fixture.detectChanges();
    await fixture.whenStable();

    const compiled = fixture.nativeElement as HTMLElement;
    expect(compiled.textContent).toContain('Bootstrap mode active');

    // Simulate successful bootstrap completion.
    status$.next({ systemState: { seedModeActive: false } });
    fixture.detectChanges();
    await fixture.whenStable();

    expect(compiled.textContent).not.toContain('Bootstrap mode active');
  });
});
